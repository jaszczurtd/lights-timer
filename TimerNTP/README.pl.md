# Firmware TimerNTP

TimerNTP steruje przekaźnikami na Pico W. Obsługuje lokalne przyciski, dzienny
harmonogram, wyświetlacz OLED i pomiar temperatury oraz wymienia polecenia i stan
przez MQTT. Formaty wiadomości i działanie harmonogramu opisuje
[dokumentacja MQTT](../doc/pl/mqtt.md).

## Przygotowanie komputera

Zachowaj taki układ katalogów:

```text
projects_git/
  lights-timer/
    TimerNTP/
  libraries/
    JaszczurHAL/
    Credentials/
```

Sklonuj [JaszczurHAL](https://github.com/jaszczurtd/JaszczurHAL) do wskazanego
katalogu. Z **katalogu głównego lights-timer** przygotuj jego zależności:

```bash
(cd ../libraries/JaszczurHAL && ./runmefirst.sh)
```

Skrypt przygotowuje Pico SDK, toolchain Arm, narzędzia wgrywania i zależności
komputera. `jh-vscode` wymaga Pythona 3. Następnie wykonaj
[instrukcję Credentials](../libraries/Credentials/README.pl.md), w tym jawne
ustawienie `RP2040_TOOLCHAIN_BIN` dla szablonu. Przed kompilacją TimerNTP musi
istnieć zewnętrzne `libraries/Credentials/src/cortex-m0plus/libCredentials.a`.

## Sprzęt i konfiguracja

[device_tree.json](device_tree.json) definiuje montaż `picow`, jego target
`rp2040`, płytkę `picow`, przypisanie GPIO, magistralę I2C i adres OLED.
Podłączając sprzęt, korzystaj z wartości w `assemblies.picow`:

- Podłącz OLED SSD1306 128×32 do pinów SDA/SCL magistrali `i2c`, zgodnie
  z ustawionym adresem. Ta aplikacja wymaga magistrali I2C o indeksie 0.
- Podłącz wejścia sterujące przekaźników do `relay1`–`relay4`, właściwość
  `pins.out`. Firmware ustawia wysoki poziom GPIO dla włączonego przekaźnika;
  dobierz zgodny interfejs przekaźników. Liczba używanych przekaźników pochodzi
  z wpisu urządzenia w Credentials.
- Podłącz każdy przycisk między wejście `button1`–`button4` a masę. Wejścia
  mają pull-up i są aktywne stanem niskim.
- Podłącz linię danych DS18B20 do `thermometer.pins.data`, z rezystorem pull-up
  wymaganym przez obwód czujnika. Zasilanie i poziomy sygnałów dobierz według
  specyfikacji modułów; połącz ich masę z masą sterownika.

Aby wyświetlić aktualne połączenia montażu bez utrzymywania drugiej tabeli pinów,
uruchom z **TimerNTP**:

```bash
python3 - <<'PY'
import json
from pathlib import Path
tree = json.loads(Path("device_tree.json").read_text())
print(json.dumps(tree["assemblies"]["picow"], indent=2))
PY
```

[Manifest projektu](.vscode/jaszczurhal.project.json) wybiera `assembly: picow`
i zawiera ustawienia CMake, identyfikacji USB, plików wynikowych i OTA.
[hal_project_config.h](hal_project_config.h) wybiera funkcje HAL. Połączenia
zmieniaj w device tree i ponownie kompiluj; nie edytuj wygenerowanego
`jh_hardware.h`. Ustawienia lokalne mogą zapisywać montaż i port wgrywania.
Nadpisania target/board ze starszego sposobu wyboru nie obowiązują z tym device tree.

## Kompilacja i wgrywanie

Otwórz **TimerNTP** jako katalog roboczy VS Code. Zadania w
[tasks.json](.vscode/tasks.json) obsługują kompilację, kompilację debug, wgrywanie
przez port szeregowy/UF2/OTA, wykrywanie OTA, monitor szeregowy, odświeżenie
IntelliSense i reset identyfikacji USB. `Project: Select board` w projekcie
z device tree wybiera montaż.

Poniższe polecenia wykonuj z **TimerNTP**. Jawny wybór montażu pozwala również
uniknąć niejednoznaczności spowodowanej starymi lokalnymi ustawieniami target/board:

```bash
../../libraries/JaszczurHAL/vscode/entry/jh-vscode config-dump --project . --assembly picow
../../libraries/JaszczurHAL/vscode/entry/jh-vscode check-hardware --project .
../../libraries/JaszczurHAL/vscode/entry/jh-vscode build --project . --assembly picow
```

Oczekiwany wynik to target `rp2040`, płytka i montaż `picow`, brak błędów
konfiguracji sprzętu i udane linkowanie. Główne pliki wynikowe trafiają do
`.build/rp2040/picow/`: `firmware.elf`, `firmware.bin`, `firmware.uf2`
i `firmware.ota`. `compile_commands_patched.json` w tym samym katalogu zasila
IntelliSense. Wyniki CMake i `firmware.elf.map` pozostają pod `.build/cmake/`.

Przy pierwszej instalacji podłącz Pico W przez USB, trzymając BOOTSEL,
a następnie wgraj obraz UF2:

```bash
../../libraries/JaszczurHAL/vscode/entry/jh-vscode upload-uf2 --project . --assembly picow
```

Jeśli sterownik ma już zgodny firmware, użyj wybranego portu szeregowego.
W razie potrzeby wypisz porty i wybierz właściwy przez `Project: Change port`:

```bash
../../libraries/JaszczurHAL/vscode/entry/jh-vscode list-ports --project . --assembly picow
../../libraries/JaszczurHAL/vscode/entry/jh-vscode upload --project . --assembly picow
../../libraries/JaszczurHAL/vscode/entry/jh-vscode monitor --project . --assembly picow
```

Dla kompilacji debug użyj `build-debug` zamiast `build`, a do odświeżenia bazy
edytora — `refresh-intellisense`. Jeśli żaden port szeregowy nie pasuje, przed
ponowną próbą sprawdź przewód, wybór portu i identyfikację USB.

## Start i obsługa lokalna

Firmware ustawia CET/CEST przed odtworzeniem poprawnego czasu z zegara RP AON.
Następnie łączy Wi-Fi i wymaga świeżego wyniku NTP przed uruchomieniem WireGuard
i MQTT. Zweryfikowany czas NTP jest zapisywany do AON. Ten zegar może przetrwać
restart przy zachowanym zasilaniu; nie ma podtrzymania bateryjnego.

Wbudowana dioda miga podczas łączenia z Wi-Fi i synchronizacji NTP, a świeci
ciągle w `STATE_CONNECTED`. Log szeregowy pozwala odróżnić problemy NTP,
WireGuard i MQTT. Wykrywanie może odpowiadać już po połączeniu Wi-Fi,
przed zakończeniem całego startu MQTT.

Stany przekaźników i harmonogram są zapisywane we flash. Po restarcie innym
niż reset watchdogiem firmware początkowo pomija zapisany stan ON pierwszego
przekaźnika; harmonogram może go przywrócić, gdy dostępny jest czas. Po resecie
watchdogiem odtwarzany jest także stan tego przekaźnika. Pozostałe stany
przekaźników są przywracane normalnie.

KV korzysta z całego obszaru EEPROM zarezerwowanego przez HAL. Wersja dla Pico W
rezerwuje 8192 bajty na dwa niezależnie kasowalne banki. Przy starcie wypisuje
`KV ready: 8192 bytes`. Błędy inicjalizacji, zapisu harmonogramu i zatwierdzania
zapisu zawierają kod błędu HAL w logu szeregowym. Po aktualizacji firmware
używającego starego układu 512 B sprawdź harmonogram i stany przekaźników,
a w razie potrzeby ustaw je ponownie; ten układ nie ma ścieżki migracji.

Gdy OLED śpi, pierwsze naciśnięcie i zwolnienie przycisku tylko go budzi.
Przy aktywnym ekranie zwolnienie przycisku przełącza jego przekaźnik i zapisuje
stan. Polecenia MQTT i zmiany przekaźników również budzą ekran. Czas aktywności
wyświetlacza jest określony w [Config.h](Config.h).

## Aktualizacje OTA

Pierwszą instalację wykonaj przez USB. Kolejne aktualizacje mogą używać OTA
przez LAN, gdy dostępne jest Wi-Fi, nawet przed zakończeniem startu MQTT.

Przed wgraniem sprawdź ustawienia `ota` w manifeście: zawierają adres urządzenia
i adres rozgłoszeniowy konkretnej instalacji. `--host` wybiera inne urządzenie
dla jednego polecenia. Gdy host jest ustawiony, wykrywanie pyta bezpośrednio ten
adres; usuń ustawienie hosta, aby użyć skonfigurowanego rozgłoszenia LAN.
To wykrywanie jest niezależne od listy urządzeń zbieranej przez provider.

Urządzenie odbiera UDP na `ota.port`, który musi odpowiadać `CR_OTA_UPDATE_PORT`.
Następnie otwiera połączenie TCP do komputera wysyłającego obraz, na
`ota.listenPort`. Oba porty mają tu domyślnie wartość 8266; komputer odbiera też
odpowiedzi wykrywania UDP na `listenPort`. Dopuść ten ruch LAN w sieci urządzenia
i firewallu komputera. Broker potrzebuje tego portu TCP tylko wtedy, gdy sam
jest komputerem wysyłającym obraz.

Z **TimerNTP** podaj adres LAN urządzenia i takie samo hasło jak
`CR_OTA_UPDATE_PASSWORD`:

```bash
read -r -p 'Device LAN IP: ' OTA_HOST
../../libraries/JaszczurHAL/vscode/entry/jh-vscode ota-discover --project . --assembly picow --host "$OTA_HOST"
read -r -s -p 'OTA password: ' TIMER_NTP_OTA_PASSWORD
echo
export TIMER_NTP_OTA_PASSWORD
../../libraries/JaszczurHAL/vscode/entry/jh-vscode upload-ota --project . --assembly picow --host "$OTA_HOST"
unset TIMER_NTP_OTA_PASSWORD
```

Wgrywanie tworzy `firmware.signed.ota` w katalogu wynikowym montażu.
Uwierzytelnianie nie szyfruje kontenera firmware. `JH_OTA_GENERATION`
i `JH_OTA_VERSION` w manifeście opisują wydanie; numer generacji jest metadanymi,
a nie wymuszanym licznikiem ochrony przed starszym obrazem.

Próbny obraz jest potwierdzany dopiero w `STATE_CONNECTED`, po uruchomieniu
Wi-Fi, NTP, WireGuard i MQTT. Brak tego stanu pozostawia obraz niepotwierdzony
i może spowodować rollback po wyczerpaniu limitu prób startu. Po wgraniu sprawdź
logi startu; samo zakończenie transferu nie potwierdza poprawnego uruchomienia.
Jeśli odzyskanie dostępu przez sieć jest niemożliwe, użyj USB/BOOTSEL.

## Hostowy test zapisu ustawień

Na Linuksie z GCC lub Clang uruchom z **katalogu głównego repozytorium**:

```bash
cmake -S ../libraries/JaszczurHAL -B ../libraries/JaszczurHAL/.build/lights-timer-kv
cmake --build ../libraries/JaszczurHAL/.build/lights-timer-kv --target hal_mock -j4
cmake -S tests -B .build/tests
cmake --build .build/tests -j4
ctest --test-dir .build/tests --output-on-failure
```

Test kompiluje obsługę MQTT i kod sprzętowy TimerNTP z produkcyjną implementacją
flash/KV dla RP, działającą na buforze RAM. Sprawdza zmianę harmonogramu,
publikowany status, odczyt po ponownej inicjalizacji oraz zapis stanów
przekaźników. Korzysta z przykładowych danych Credentials i mocków peryferiów;
nie wymaga płytki ani brokera. Nie sprawdza czasu zapisu fizycznej pamięci flash
ani aplikacji mobilnej.

## Szczegóły implementacji

[TimerNTP.cpp](TimerNTP.cpp) dostarcza `app_start()` i `app_task0()`;
JaszczurHAL zapewnia punkt wejścia i wywołuje pętlę aplikacji. HAL dostarcza
obsługę sieci CYW43/lwIP, WireGuard, czasu, OTA, watchdoga, KV/EEPROM,
LittleFS, I2C, SSD1306, DS18B20 i cJSON. Szczegóły opisują
[API HAL](https://github.com/jaszczurtd/JaszczurHAL/blob/main/doc/pl/JaszczurHAL_API.md),
[instrukcja projektu firmware](https://github.com/jaszczurtd/JaszczurHAL/blob/main/doc/pl/FwProjectWorkflow.md)
i [instrukcja OTA](https://github.com/jaszczurtd/JaszczurHAL/blob/main/doc/pl/OTAWorkflow.md).
