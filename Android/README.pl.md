# Aplikacja Android

Aplikacja pokazuje sterowniki wykryte przez provider, przełącza ich przekaźniki
i ustawia dzienny harmonogram pierwszego przekaźnika. Korzysta z Javy, części
interfejsu napisanych w Kotlinie i klienta MQTT Eclipse Paho. Wymaga Androida
7.0 lub nowszego (API 24).

## Kompilacja i połączenie

1. Przygotuj [broker i jego certyfikat TLS](../doc/pl/network.md).
2. Umieść certyfikat CA weryfikujący łańcuch brokera w
   `app/src/main/res/raw/ca.crt`, licząc od tego katalogu. W razie potrzeby utwórz
   katalog `raw`. Dostarcz pojedynczy certyfikat CA X.509 w formacie PEM;
   plik jest ignorowany przez Git i wczytywany jako `R.raw.ca`.
3. Otwórz ten katalog `Android` w Android Studio i zsynchronizuj Gradle. Użyj
   Gradle JDK obsługiwanego przez Android Gradle Plugin projektu i zainstaluj
   SDK wymagane w [app/build.gradle.kts](app/build.gradle.kts).
4. Skompiluj aplikację i uruchom ją na telefonie. Podaj nazwę hosta brokera bez
   schematu i portu oraz użytkownika i hasło MQTT. Możesz podać adres IP, jeśli
   certyfikat serwera obejmuje ten adres.

Aplikacja zawsze łączy się z `ssl://<broker>:8883`. Pobiera listę urządzeń od
providera i subskrybuje tematy stanu sterowników. Pusta lista może oznaczać, że
wykrywanie nie znalazło jeszcze sterownika. Sprawdź
[provider](../RaspberryPi/aqua_topic_provider/README.pl.md) i
[wiadomości MQTT](../doc/pl/mqtt.md), aby odróżnić to od problemu z brokerem.

## Obsługa certyfikatów

[MQTTClient.java](app/src/main/java/com/example/aquacontrol/MQTTClient.java)
tworzy magazyn zaufanych certyfikatów zawierający certyfikat z `R.raw.ca`.
Także dla certyfikatu serwera wystawionego przez publiczne CA trzeba dostarczyć
odpowiednie CA do tego magazynu; zwykła ścieżka połączenia nie korzysta z pełnego
systemowego magazynu CA telefonu.

Jeśli przygotowanie tej konfiguracji TLS zgłosi wyjątek, aplikacja zapisuje
`SSL problem` w logu i nadal wywołuje `connect()`, bez własnej fabryki socketów.
Taki wpis oznacza błąd konfiguracji. Obecna implementacja nie gwarantuje użycia
dołączonego CA przy każdym połączeniu.

Aplikacja nie wczytuje certyfikatu klienta i jego klucza prywatnego. Na jej
porcie brokera pozostaw `require_certificate false`. Włączenie mTLS wymaga
zmian aplikacji i przygotowania danych uwierzytelniających klienta.

## Harmonogram i stan

[Dokumentacja MQTT](../doc/pl/mqtt.md) opisuje jednostki, strefę czasową,
granice harmonogramu i współdziałanie ze sterowaniem ręcznym. Firmware używa
CET/CEST. Android do natychmiastowego podglądu harmonogramu używa lokalnego zegara
telefonu, więc w innej strefie czasowej może przez chwilę pokazać odmienny stan,
zanim nadejdzie wiadomość ze sterownika. Stan przesłany przez sterownik jest
informacją o stanie urządzenia.
