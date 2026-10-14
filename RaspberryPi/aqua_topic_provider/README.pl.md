# Provider urządzeń

Provider wykrywa sterowniki TimerNTP w sieci LAN i publikuje ich listę przez
MQTT. Android używa tej listy do wyświetlania dostępnych urządzeń. Przed
uruchomieniem przygotuj [sieć i broker](../../doc/pl/network.md).

## Kompilacja

Na serwerze Raspberry Pi wykonaj poniższe polecenia z katalogu
`lights-timer/RaspberryPi/aqua_topic_provider`:

```bash
sudo apt update
sudo apt install -y build-essential libmosquitto-dev libmosquitto1 \
  mosquitto-clients libcjson-dev
```

Przed kompilacją sprawdź [provider.h](provider.h). Plik określa adres
rozgłoszeniowy LAN, port i odstęp między próbami wykrywania, adres i port MQTT,
temat oraz identyfikator klienta. Adres rozgłoszeniowy musi odpowiadać sieci
sterowników, a adres MQTT musi prowadzić do brokera przez WireGuard.
Po zmianie tych stałych ponownie skompiluj program:

```bash
make
```

Wynikiem jest plik wykonywalny `provider` w tym katalogu. Program linkuje
systemowe biblioteki Mosquitto i cJSON.

## Utworzenie pliku danych MQTT

Sprawdź [mqtt-devices-provider.service](mqtt-devices-provider.service) i wybierz
konto, które ma uruchamiać usługę (`User`). Z tego konta uruchom w katalogu programu:

```bash
./provider
```

Przy pierwszym uruchomieniu interaktywnym program pyta o użytkownika i hasło MQTT,
a następnie zapisuje je w `~/.mqtt_provider_auth` tego konta, z uprawnieniami
`0600`. Zatrzymaj go przez `Ctrl+C` po utworzeniu pliku i udanym połączeniu
z brokerem. Pusta lista urządzeń jest normalna przed uruchomieniem sterowników.

Utwórz ten plik przed włączeniem usługi: systemd nie odpowie na pytanie o login.
Usługa nie użyje danych zapisanych w katalogu domowym innego konta.

## Instalacja usługi

Dostosuj `ExecStart`, `WorkingDirectory` i `User` w pliku usługi do swojej
instalacji. Usługa wymaga `wg-quick@wg0.service`; najpierw skonfiguruj ten
interfejs. Jeśli Twój interfejs WireGuard ma inną nazwę, zmień obie zależności
w pliku usługi.

Z tego samego katalogu zainstaluj i uruchom usługę:

```bash
sudo cp mqtt-devices-provider.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now mqtt-devices-provider.service
sudo systemctl status mqtt-devices-provider.service
sudo journalctl -u mqtt-devices-provider.service -n 50 --no-pager
```

Oczekiwany wynik to aktywna usługa i komunikaty o udanym połączeniu MQTT. Jeśli
usługa stale się restartuje, sprawdź plik danych konta `User`, WireGuard,
dostęp do brokera oraz ścieżki w pliku usługi.

## Wykrywanie i sprawdzenie działania

Provider rozsyła `AQUA_DISCOVER` i zbiera odpowiedzi `AQUA_FOUND|...`. Przy
sprawdzaniu dostępności uznaje za odpowiedź zarówno zestawienie połączenia TCP,
jak i jego odmowę przez urządzenie; nie potwierdza to działania połączenia MQTT.

Lista urządzeń jest publikowana z flagą retained w `discovered/devices`.
Możesz ją sprawdzić subskrypcją opisaną w [dokumentacji MQTT](../../doc/pl/mqtt.md).
Osobno sprawdź temat stanu sterownika. Jeśli lista jest pusta, sprawdź adres
rozgłoszeniowy, wspólny segment LAN i reguły wykrywania w firewallu.
