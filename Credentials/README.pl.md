# Szablon biblioteki Credentials

[English](README.md)

Szablon implementuje publiczne API Credentials bez sekretów autora i jego
prywatnego kodowania danych. Uzupełnij własne wartości i zbuduj archiwum dla
platformy firmware'u. Dane są przechowywane bez szyfrowania.

TimerNTP linkuje `../Credentials/build/<target>/libCredentials.a` dla platformy
swojego montażu (`rp2040` dla `picow`), licząc od katalogu głównego lights-timer.
Ten szablon nie jest linkowany bezpośrednio.

## Konfiguracja prywatnej kopii

Przy nowej instalacji uruchom z **katalogu głównego lights-timer**:

```bash
./scripts/setup-credentials.sh
```

Instalator kopiuje ten katalog do `../Credentials`, tworzy dwa lokalne pliki
konfiguracji i odmawia nadpisania istniejącego katalogu. Jeśli masz już w tym
miejscu działającą prywatną bibliotekę, korzystaj z jej sposobu konfiguracji
i kompilacji. Dalsze polecenia wykonuj w tym zewnętrznym katalogu Credentials:

```bash
cd ../Credentials
```

Jeśli kopiujesz szablon samodzielnie, uruchom z katalogu Credentials:

```bash
./scripts/configure.sh
```

```powershell
py -3 scripts/configure.py
```

Edytuj oba pliki: `config/CredentialsData.local.h` oraz
`config/MacHostMapping.local.cpp`. Zastąp przykładowe wartości i ustaw
`CREDENTIALS_LOCAL_CONFIGURED` na `1`. Zwykła konfiguracja zachowuje istniejące
pliki lokalne. Nie dodawaj konfiguracji prywatnej ani archiwów do Git.

Dla TimerNTP:

- W `config/CredentialsData.local.h` wpisz dane Wi-Fi i MQTT, serwera
  WireGuard, serwerów NTP oraz hasło i port OTA. TimerNTP łączy się
  z `CR_MQTT_BROKER_WIREGUARD` na `CR_MQTT_BROKER_PORT`; pola brokera TLS, CA
  i SIM należą do współdzielonego API, TimerNTP ich nie używa.
- W `config/MacHostMapping.local.cpp` dodaj fabryczny MAC Wi-Fi każdego
  sterownika, unikalną nazwę hosta, liczbę przekaźników od 1 do 4, adres
  WireGuard i klucz prywatny. Odpowiadający mu klucz publiczny i adres
  zarejestruj na serwerze WireGuard.

Porównywanie adresów MAC pomija separatory i wielkość liter. MAC można odczytać
z listy klientów punktu dostępowego po połączeniu Pico W z Wi-Fi. Można też użyć
programu diagnostycznego wywołującego `hal_wifi_get_mac()` po inicjalizacji
Wi-Fi; przed użyciem wyniku trzeba sprawdzić wartość zwróconą przez funkcję.

## Kompilacja dla platformy firmware'u

```bash
./scripts/build.sh rp2040
```

```powershell
py -3 scripts/build.py rp2040
```

| Platforma | Archiwum |
| --- | --- |
| `rp2040`, `rp2350-arm`, `rp2350-riscv`, `stm32g474`, `esp32`, `esp32s3` | `build/<target>/libCredentials.a` |

Skrypt szuka JaszczurHAL obok zainstalowanego katalogu Credentials. Przy innym
układzie podaj `--jaszczurhal-root /path/to/JaszczurHAL`. Korzysta z CMake,
Ninja i kompilatorów zarządzanych przez HAL, jeśli są dostępne; w przeciwnym
razie szuka narzędzi w `PATH`.
Podczas budowania dla RP i STM32 dodaje katalog wybranego kompilatora na
początek `PATH` procesów kompilacji. Dzięki temu wewnętrzne testy CMake
znajdują te same narzędzia, bez zmiany środowiska Twojej powłoki.

Platformy RP używają toolchainu Pico SDK wybranego dla danej platformy,
a STM32 — pliku toolchainu HAL. `rp2350-riscv` korzysta z wersji narzędzi
RISC-V wskazanej przez HAL. Kompilacja ESP używa ESP-IDF z HAL i jego domyślnej
konfiguracji, w tym picolibc; potrzebne narzędzia są przygotowywane przy
pierwszym użyciu. Archiwum buduj dla tego samego układu i zgodnej konfiguracji
SDK/ABI co firmware.

Obecny JaszczurHAL obsługuje archiwa przez `JH_LINK_LIBRARIES` i nagłówki przez
`JH_EXTRA_INCLUDES` na RP, STM32 oraz ESP. Ustaw te ścieżki w manifeście aplikacji.
Manifest TimerNTP ma jeden montaż, `picow` (RP2040); pozostałe platformy
biblioteki służą innym projektom. Po każdej zmianie danych albo mapowania MAC
przebuduj archiwum, a potem TimerNTP.

## API i czas życia buforów

Używaj nagłówka i archiwum z tej samej kompilacji biblioteki. Publiczny nagłówek
to [src/Credentials.h](src/Credentials.h); nagłówek główny dołącza ten plik.

- `getCredential()` zwraca zaalokowany tekst albo `NULL` przy błędnym ID lub
  nieudanej alokacji. Wymaż sekret i zwolnij bufor przez `free()` po użyciu.
- `getWireguardPrivateKey()` także zwraca zaalokowany bufor, mimo typu
  `const char *`; wywołujący musi go wymazać i zwolnić. Nieznany MAC daje `NULL`.
- `getFriendlyHostname()` i `getWireguardLocalIP()` zwracają tekst należący do
  biblioteki. Nie zwalniaj go. Zastępcza nazwa hosta korzysta ze współdzielonego
  bufora statycznego, nadpisywanego przy kolejnym takim odczycie; brak IP daje `NULL`.
- `getCredentialInt()` konwertuje przez `atoi()` i zwraca `-1`, gdy nie uzyska
  tekstu. Nie sprawdza składni liczby. `getSwitchesNumber()` zwraca `0` dla
  nieznanego MAC. `initCredentials()` w tym szablonie nic nie wykonuje.

## Publiczna konfiguracja testowa

`scripts/configure.py --test` używa publicznych danych testowych z zastrzeżonymi
adresami, bez rzeczywistych sekretów. Odmawia działania, gdy istnieje choć
jeden plik konfiguracji lokalnej. Używaj tych wartości tylko w automatycznych
testach kompilacji, nie w urządzeniu.
