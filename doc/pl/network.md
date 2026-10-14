# Sieć i broker MQTT

Sterowniki Pico W łączą się z Mosquitto przez WireGuard na TCP 1883. Android
łączy się przez TLS na TCP 8883. Provider wykrywa sterowniki bezpośrednio
w LAN, dlatego musi być w tej samej domenie rozgłoszeniowej, nawet gdy MQTT
korzysta z tunelu.

Ta instalacja używa serwera Raspberry Pi i IPv4. TimerNTP korzysta z WireGuard
ze względu na istniejącą sieć oraz jako praktyczne zastosowanie tej implementacji
HAL. HAL obsługuje również MQTT przez TLS; użycie go w TimerNTP wymagałoby zmian
firmware, danych dostępowych i routingu do brokera.

## Wymagania wstępne

Przed uruchomieniem brokera przygotuj:

- Działający interfejs serwera WireGuard `wg0`, w tym przykładzie z adresem
  `10.8.0.1/24`. Dla każdego sterownika skonfiguruj peera z kluczem publicznym
  i unikalnym adresem tunelu, zgodnymi z
  [ustawieniami Credentials](../../libraries/Credentials/README.pl.md).
  [Instrukcja WireGuard](https://www.wireguard.com/quickstart/) opisuje
  generowanie kluczy i konfigurację interfejsu.
- Łączność LAN między providerem a sterownikami, w tym rozgłoszenia UDP.
  Ustaw adres rozgłoszeniowy swojej podsieci w
  [provider.h](../../RaspberryPi/aqua_topic_provider/provider.h).
  Wykrywanie OTA ma osobne ustawienia w manifeście projektu firmware.
- Dostęp sterowników do DNS i NTP przed zestawieniem tunelu. TimerNTP wymaga
  świeżej synchronizacji NTP przed uruchomieniem WireGuard.
- Nazwę hosta brokera osiągalną z Androida, z DNS i przekierowaniem portu
  na routerze, jeśli serwer jest za NAT.
- Łańcuch certyfikatów serwera w PEM obejmujący tę nazwę hosta, odpowiadający
  mu klucz prywatny i certyfikat CA weryfikujący łańcuch. Uzyskaj je od swojego
  publicznego lub prywatnego CA. Mosquitto musi móc użyć klucza bez interakcji
  z użytkownikiem. To samo CA zastosuj w próbach CLI i aplikacji Android.

Na serwerze sprawdź WireGuard przed przejściem dalej:

```bash
sudo systemctl status wg-quick@wg0.service
ip -4 address show dev wg0
sudo wg show wg0
```

Oczekiwany wynik to aktywny interfejs z właściwym adresem i skonfigurowanymi
peerami. Handshake pojawi się dopiero po połączeniu peera.

## Instalacja i konfiguracja Mosquitto

Polecenia serwera wykonuj z dowolnego katalogu w Raspberry Pi OS lub Debianie:

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
sudo mosquitto_passwd -c /etc/mosquitto/passwd your_username
sudo chown root:mosquitto /etc/mosquitto/passwd
sudo chmod 640 /etc/mosquitto/passwd
```

Zastąp `your_username` nazwą konta MQTT. Używaj `-c` tylko przy tworzeniu pliku
haseł; pomiń tę opcję przy dodawaniu użytkownika do istniejącego pliku.

Zainstaluj przygotowane wcześniej certyfikaty, zastępując ścieżki `/path/to/...`:

```bash
sudo install -d -o root -g mosquitto -m 750 /etc/mosquitto/certs
sudo install -o root -g mosquitto -m 640 /path/to/server-chain.crt /etc/mosquitto/certs/server.crt
sudo install -o root -g mosquitto -m 640 /path/to/server.key /etc/mosquitto/certs/server.key
sudo install -o root -g root -m 644 /path/to/ca.crt /etc/mosquitto/ca.crt
```

Użyj jednego pliku `/etc/mosquitto/conf.d/lights-timer.conf` dla obu portów:

```ini
allow_anonymous false
password_file /etc/mosquitto/passwd

listener 1883 10.8.0.1
protocol mqtt

listener 8883
socket_domain ipv4
protocol mqtt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
```

Główna konfiguracja Mosquitto musi wczytywać `/etc/mosquitto/conf.d`.
Zastąp wcześniejszą konfigurację tych portów, zamiast wczytywać obie kopie.
Powyższe ustawienia uwierzytelniania dotyczą obu portów; przykład nie włącza
osobnych ustawień uwierzytelniania dla każdego listenera.

TCP 1883 nasłuchuje wyłącznie na adresie WireGuard. Port TLS uwierzytelnia
użytkownikiem i hasłem MQTT. Android nie dostarcza certyfikatu klienta, więc
włączenie mTLS wymagałoby zmian aplikacji. Obsługę CA i obecne zachowanie
przy błędach opisuje [instrukcja Androida](../../Android/README.pl.md).
Opcje portów, certyfikatów i uwierzytelniania opisuje
[dokumentacja Mosquitto](https://mosquitto.org/man/mosquitto-conf-5.html).

Ponieważ broker nasłuchuje na adresie tunelu, uruchamiaj go po WireGuard.
Wykonaj `sudo systemctl edit mosquitto` i dodaj takie rozszerzenie jednostki:

```ini
[Unit]
Requires=wg-quick@wg0.service
After=wg-quick@wg0.service
```

Następnie uruchom broker i sprawdź logi:

```bash
sudo systemctl daemon-reload
sudo systemctl enable mosquitto
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
sudo journalctl -u mosquitto -n 50 --no-pager
sudo ss -lntup
```

Oczekiwany wynik to aktywna usługa, TCP 1883 na `10.8.0.1` i TCP 8883
nasłuchujący po IPv4. Przed startem providera usuń problemy z uprawnieniami
certyfikatów, powtórzonymi portami lub brakującym `wg0`.

## Ruch i firewall

Tabela wskazuje odbiorcę połączenia; ruch OTA nie musi przechodzić przez
serwer brokera.

| Odbiorca | Port/protokół | Źródło i przeznaczenie |
| --- | --- | --- |
| Serwer WireGuard | 51820/UDP | Sieć sterownika, punkt końcowy tunelu; zgodnie ze skonfigurowanym portem |
| Mosquitto | 1883/TCP | Peery WireGuard i provider na serwerze; bez dostępu z publicznego Internetu |
| Mosquitto | 8883/TCP | Android i klienci TLS |
| Sterowniki i provider | 12345/UDP | Żądania i odpowiedzi wykrywania w LAN |
| Sterownik | 8266/UDP | Komputer wysyłający OTA; zgodnie z `ota.port` i portem firmware |
| Komputer wysyłający OTA | 8266/TCP i UDP | Połączenie TCP inicjowane przez sterownik i odpowiedzi wykrywania UDP; zgodnie z `ota.listenPort` |

Jeśli zarządzasz firewallem serwera bezpośrednio przez iptables, poniższy
**przykład pierwszej instalacji dla IPv4** filtruje porty projektu bez zmiany
polityki dla pozostałego ruchu. Ustaw `LT_LAN_IF` i `LT_LAN_SUBNET` zgodnie
ze swoją siecią. Jeśli serwerem zarządza już menedżer firewalla, wprowadź
w nim równoważne reguły. Jeśli `LIGHTS_TIMER` już istnieje, zmień jego reguły
zamiast ponownie tworzyć łańcuch.

```bash
LT_LAN_IF=eth0
LT_LAN_SUBNET=192.168.2.0/24
sudo iptables -N LIGHTS_TIMER
sudo iptables -A LIGHTS_TIMER -i lo -p tcp --dport 1883 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -i wg0 -s 10.8.0.0/24 -p tcp --dport 1883 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -p tcp --dport 1883 -j DROP
sudo iptables -A LIGHTS_TIMER -i "$LT_LAN_IF" -s "$LT_LAN_SUBNET" -p udp --dport 12345 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -p udp --dport 12345 -j DROP
sudo iptables -A LIGHTS_TIMER -p udp --dport 51820 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -p tcp --dport 8883 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -j RETURN
sudo iptables -I INPUT 1 -j LIGHTS_TIMER
```

Jawne reguły DROP odrzucają pozostałe źródła dla portów wewnętrznych także
wtedy, gdy polityką INPUT jest ACCEPT. Istotne jest umieszczenie tego łańcucha
przed szerszymi regułami dopuszczającymi ruch; kolejność reguł i polityki
łańcuchów opisuje [dokumentacja iptables](https://man7.org/linux/man-pages/man8/iptables.8.html).
Przykład zakłada dozwolony ruch wychodzący. Dla instalacji IPv6 osobno
skonfiguruj odpowiednie porty nasłuchu i filtrowanie.

Jeśli zaufani klienci LAN również potrzebują MQTT bez TLS, dodaj osobny listener
na adresie LAN serwera i regułę dopuszczającą ten interfejs i podsieć przed
regułą DROP dla TCP 1883. Nie obejmuj tym dostępu z interfejsu publicznego.

OTA wymaga reguł na **komputerze wysyłającym obraz**, dla odpowiedzi i połączenia
TCP przychodzącego z adresu LAN sterownika. Dopuść je wyłącznie we właściwej
sieci LAN; zobacz [procedurę OTA firmware](../../TimerNTP/README.pl.md#aktualizacje-ota).

Jeśli serwer używa `netfilter-persistent`, po sprawdzeniu dostępu zapisz reguły:

```bash
sudo netfilter-persistent save
```

Polecenie wymaga pakietu utrwalania reguł; w Debianie dostarcza je
`netfilter-persistent`, zwykle instalowany razem z `iptables-persistent`.

## Sprawdzenie i dalsze kroki

Wykonaj [próby połączenia MQTT](mqtt.md#sprawdzenie-połączenia) dla obu portów.
Subskrybent powinien wypisać wiadomość wysłaną przez publikującego. Następnie
zainstaluj [provider](../../RaspberryPi/aqua_topic_provider/README.pl.md),
przygotuj [firmware](../../TimerNTP/README.pl.md) i skonfiguruj
[Androida](../../Android/README.pl.md).
