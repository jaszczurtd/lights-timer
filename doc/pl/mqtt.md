# Wiadomości MQTT i harmonogram

MQTT przenosi listę urządzeń, polecenia przekaźników, dzienny harmonogram
i raportowany stan. Adresy brokera i sposób dostępu opisuje
[instrukcja sieci](network.md). Każdy sterownik wymaga unikalnej nazwy hosta
w mapowaniu Credentials; `{hostname}` poniżej oznacza tę nazwę, a nie adres IP.

## Tematy

| Temat | Publikujący → odbiorca | Retained |
| --- | --- | --- |
| `discovered/devices` | Provider → Android | Tak |
| `switch-{hostname}` | Android → sterownik | Nie, w aplikacji Android |
| `time-{hostname}` | Android → sterownik | Nie, w aplikacji Android |
| `status-{hostname}` | Sterownik → Android | Tak |

Wysyłaj polecenia bez retain, aby broker nie odtwarzał starego polecenia po
ponownym połączeniu. Stan lub lista z flagą retained to ostatnia opublikowana
wartość; samo jej odebranie nie dowodzi, że sterownik jest obecnie połączony.

## Lista urządzeń

Przykładowa wiadomość `discovered/devices`:

```json
{"devices":[{"mac":"AA:BB:CC:DD:EE:01","ip":"192.168.2.171","hostName":"lamp1","switches":4}]}
```

`ip` to adres LAN sterownika. `hostName` jest sufiksem jego tematów,
a `switches` to liczba przekaźników zgłaszana na podstawie Credentials.
Pusta lista ma postać `{"devices":[]}`. Wykrywanie i sprawdzanie dostępności
opisuje [instrukcja providera](../../RaspberryPi/aqua_topic_provider/README.pl.md).

## Polecenie przekaźników

Aby zmienić wybrane przekaźniki, opublikuj w `switch-lamp1`:

```json
{"isOn1":true,"isOn3":false}
```

Klucze mają numery od `isOn1` do `isOn4`. Używaj wartości logicznych JSON.
Firmware obsługuje skonfigurowaną liczbę przekaźników; pominięte klucze
pozostawiają ich stan bez zmian. Zmienione stany są zapisywane, a firmware
zleca publikację stanu.

## Dzienny harmonogram

Opublikuj oba pola w `time-lamp1`, na przykład:

```json
{"dateHourStart":300,"dateHourEnd":600}
```

To ustawia okno aktywności pierwszego przekaźnika na **05:00–10:00 CET/CEST**.
Wartości są całkowitymi liczbami minut od lokalnej północy, a nie godzinami
ani znacznikami czasu Unix. Używaj wartości od 0 do 1440; 1440 oznacza koniec
doby, już poza aktywnym przedziałem. W Androidzie można wybrać godzinę
od 00:00 do 23:59.

Przedział obejmuje początek i wyklucza koniec: `start <= now < end`.
Równe końce oznaczają puste okno. Przedział przechodzący przez północ nie jest
obsługiwany; początek późniejszy niż koniec nie tworzy nocnego harmonogramu.
Firmware używa CET/CEST z automatyczną zmianą czasu letniego, niezależnie
od strefy czasowej telefonu.

Harmonogram steruje tylko **pierwszym przekaźnikiem**. Pozostałe korzystają
z ręcznych poleceń i przycisków. Ręczna zmiana pierwszego przekaźnika pozostaje
do następnego zaobserwowanego wejścia w okno harmonogramu lub wyjścia z niego.
Zmiana harmonogramu również sprawdza nowe okno i może wywołać takie przejście.
Gdy stan okna pozostaje taki sam, firmware nie nadpisuje stale zmian ręcznych.

Firmware zapisuje oba końce przedziału i używa ich po restarcie.
[Opis startu](../../TimerNTP/README.pl.md#start-i-obsługa-lokalna) wyjaśnia
dostępność czasu i przywracanie stanów przekaźników.

## Raportowany stan

`status-{hostname}` raportuje następujące pola:

| Pole | Znaczenie |
| --- | --- |
| `status` | `ok` w zwykłym raporcie; `JSON build failed` w zastępczym raporcie błędu |
| `build` | Ciąg znaków z datą i czasem kompilacji firmware |
| `dateHourStart`, `dateHourEnd` | Zapisane końce harmonogramu w minutach |
| `isOn1` … `isOn4` | Cztery pola logiczne stanów przekaźników; interfejs powinien używać zgłaszanej liczby przekaźników |
| `localTime` | Lokalna data i czas jako `DD/MM/YYYY HH:MM:SS` lub `not available yet.` |
| `localMillis` | Milisekundy od startu, nie znacznik czasu kalendarzowego |
| `wifi` | Tekst do wyświetlenia w postaci `5/<strength>`; nie jest wartością RSSI w dBm |
| `temperatureAvailable` | Czy firmware ma poprawną próbkę temperatury |
| `temperatureC` | Temperatura w stopniach Celsjusza albo JSON `null`, gdy jest niedostępna |

Zastępczy raport błędu JSON zawiera tylko `status`. Wcześniejszy poprawny pomiar
temperatury może pozostać ostatnią dostępną próbką; wiadomość nie zawiera czasu
pomiaru. Firmware i provider używają cJSON do przetwarzania JSON.

## Sprawdzenie połączenia

Na komputerze do prób zainstaluj `mosquitto-clients`. Dla portu 1883 użyj samego
serwera lub peera WireGuard z dostępem do brokera. Otwórz **dwa terminale Bash**
i w każdym podaj te same dane połączenia; bieżący katalog nie ma znaczenia:

```bash
read -r -p 'Broker WireGuard IP: ' MQTT_HOST
read -r -p 'MQTT username: ' MQTT_USER
read -r -s -p 'MQTT password: ' MQTT_PASSWORD
echo
```

W terminalu 1 uruchom subskrybenta i pozostaw go działającego:

```bash
mosquitto_sub -h "$MQTT_HOST" -p 1883 -u "$MQTT_USER" -P "$MQTT_PASSWORD" \
  -t 'lights-timer/check' -v
```

Następnie opublikuj wiadomość w terminalu 2:

```bash
mosquitto_pub -h "$MQTT_HOST" -p 1883 -u "$MQTT_USER" -P "$MQTT_PASSWORD" \
  -t 'lights-timer/check' -m 'hello'
```

W terminalu 1 powinno pojawić się `lights-timer/check hello`. Wiadomość nie jest
retained, więc subskrybent musi być połączony przed publikacją. Zatrzymaj go
przez `Ctrl+C`.

Aby sprawdzić TLS, w **obu** terminalach ustaw nazwę hosta objętą certyfikatem
i ścieżkę CA. Na serwerze CA z instrukcji sieci znajduje się w
`/etc/mosquitto/ca.crt`; w razie potrzeby skopiuj je na komputer do prób:

```bash
read -r -p 'Broker TLS hostname: ' MQTT_HOST
read -r -p 'CA certificate path: ' MQTT_CA
```

Najpierw uruchom subskrybenta w terminalu 1:

```bash
mosquitto_sub -h "$MQTT_HOST" -p 8883 --cafile "$MQTT_CA" \
  -u "$MQTT_USER" -P "$MQTT_PASSWORD" -t 'lights-timer/check' -v
```

Następnie opublikuj wiadomość w terminalu 2; oczekiwany tekst jest taki sam:

```bash
mosquitto_pub -h "$MQTT_HOST" -p 8883 --cafile "$MQTT_CA" \
  -u "$MQTT_USER" -P "$MQTT_PASSWORD" -t 'lights-timer/check' -m 'hello'
```

Po uruchomieniu providera i firmware sprawdź ich raporty retained. W terminalu
skonfigurowanym dla portu 1883 ponownie ustaw adres WireGuard brokera i wpisz
nazwę urządzenia z Credentials:

```bash
read -r -p 'Broker WireGuard IP: ' MQTT_HOST
read -r -p 'Device hostname: ' DEVICE_HOSTNAME
mosquitto_sub -h "$MQTT_HOST" -p 1883 -u "$MQTT_USER" -P "$MQTT_PASSWORD" \
  -t 'discovered/devices' -t "status-$DEVICE_HOSTNAME" -v
```

Oczekiwany wynik to wpis urządzenia i raport stanu z opisanymi wyżej polami.
Lista bez raportu stanu wymaga sprawdzenia startu NTP, WireGuard i MQTT na
sterowniku. Opcje CLI opisują
[instrukcja publikującego Mosquitto](https://mosquitto.org/man/mosquitto_pub-1.html)
i [instrukcja subskrybenta](https://mosquitto.org/man/mosquitto_sub-1.html).
