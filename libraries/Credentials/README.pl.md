# Credentials

Ten szablon dostarcza dane dostępowe do sieci i dane urządzeń używane przez
TimerNTP. Udostępnia takie samo API jak prywatna biblioteka autora, bez jego
sekretów i prywatnego kodu kodowania danych.

## Instalacja zewnętrznej biblioteki

TimerNTP linkuje `../libraries/Credentials/src/cortex-m0plus/libCredentials.a`,
licząc od katalogu głównego lights-timer. Szablon wewnątrz tego repozytorium
nie jest linkowany bezpośrednio.

Przy nowej instalacji uruchom z **katalogu głównego lights-timer**:

```bash
./scripts/setup-credentials.sh
```

Skrypt kopiuje szablon do `../libraries/Credentials` i tworzy dwa lokalne pliki
konfiguracji. Odmawia nadpisania istniejącego katalogu. Jeśli masz już w tym
miejscu działającą prywatną bibliotekę, korzystaj z jej sposobu konfiguracji
i kompilacji.

Dalsze polecenia wykonuj w **zewnętrznym katalogu Credentials**. Z katalogu
głównego lights-timer przejdź do niego poleceniem:

```bash
cd ../libraries/Credentials
```

W szablonie skopiowanym osobno polecenie `./scripts/configure.sh` tworzy brakujące
lokalne pliki konfiguracji, zachowując istniejące.

## Konfiguracja instalacji

W zewnętrznej kopii edytuj:

- `config/CredentialsData.local.h`: wpisz dane Wi-Fi i MQTT, serwera WireGuard,
  serwerów NTP oraz hasło i port OTA. TimerNTP łączy się z
  `CR_MQTT_BROKER_WIREGUARD` na `CR_MQTT_BROKER_PORT`; pola brokera TLS i CA
  należą do współdzielonego API, ale to połączenie MQTT ich nie używa.
- `config/MacHostMapping.local.cpp`: dodaj fabryczny MAC Wi-Fi każdego sterownika,
  unikalną nazwę hosta, liczbę przekaźników od 1 do 4, adres WireGuard i klucz
  prywatny. Odpowiadający mu klucz publiczny i adres zarejestruj na serwerze
  WireGuard.

Zastąp wartości przykładowe i ustaw `CREDENTIALS_LOCAL_CONFIGURED` na `1`.
Porównywanie adresów MAC pomija separatory i wielkość liter. MAC można odczytać
z listy klientów punktu dostępowego po połączeniu Pico W z Wi-Fi. Można też użyć
programu diagnostycznego wywołującego `hal_wifi_get_mac()` po inicjalizacji
Wi-Fi; przed użyciem wyniku trzeba sprawdzić wartość zwróconą przez funkcję.

Lokalne pliki konfiguracji i wygenerowane archiwa są ignorowane przez Git.
Szablon umieszcza dane dostępowe w archiwum i firmware jako ciągi znaków;
nie szyfruje ich.

## Kompilacja dla RP2040

Potrzebujesz Bash, CMake w wersji co najmniej 3.20, narzędzia kompilacji
obsługiwanego przez CMake, np. Make, oraz toolchainu Arm GNU zawierającego
`arm-none-eabi-gcc`, `arm-none-eabi-g++`, `arm-none-eabi-ar` i
`arm-none-eabi-ranlib`. Użyj toolchainu przygotowanego przez JaszczurHAL dla TimerNTP.

Ustaw jawnie `RP2040_TOOLCHAIN_BIN` na jego katalog `bin`. Jeśli kompilator
jest już w `PATH`, wykonaj:

```bash
command -v arm-none-eabi-gcc
export RP2040_TOOLCHAIN_BIN="$(dirname "$(command -v arm-none-eabi-gcc)")"
./scripts/build.sh rp2040
```

Pierwsze polecenie musi wypisać ścieżkę kompilatora. Dla toolchainu spoza `PATH`
wpisz zamiast tego bezwzględną ścieżkę do `bin` w `RP2040_TOOLCHAIN_BIN`.
Bez tej zmiennej skrypt szuka pakietu RP2040 dla Arduino w
`${ARDUINO_DATA_DIR:-$HOME/.arduino15}/packages/rp2040/tools/pqt-gcc`;
brak pakietu może zakończyć skrypt bez komunikatu błędu.

Wynikiem jest `src/cortex-m0plus/libCredentials.a`. Po zmianie danych dostępowych
lub mapowania MAC ponownie skompiluj to archiwum, a następnie TimerNTP.

## Kompilacja dla STM32G474

Szablon obsługuje również ABI Cortex-M4 hard-float:

```bash
./scripts/build.sh stm32g474
```

Wynikiem jest `build/stm32g474/libCredentials.a`. Skrypt używa
`STM32_TOOLCHAIN_BIN`, jeśli zmienna jest ustawiona; w przeciwnym razie szuka
`arm-none-eabi-gcc` w `PATH`. To archiwum jest przeznaczone dla projektów STM32;
obecny montaż TimerNTP używa RP2040.
