# Lights Timer

Lights Timer pozwala sterować maksymalnie czterema przekaźnikami podłączonymi do
Raspberry Pi Pico W, z aplikacji Android lub lokalnych przycisków. Pierwszy
przekaźnik ma dzienny harmonogram. Sterownik odczytuje też temperaturę z DS18B20
i pokazuje swój stan na wyświetlaczu SSD1306.

System składa się z trzech części:

- **Firmware TimerNTP** łączy Pico W z Wi-Fi, synchronizuje zegar i wymienia
  wiadomości MQTT przez WireGuard.
- **Serwer Raspberry Pi** uruchamia WireGuard, Mosquitto i provider urządzeń.
  Provider wykrywa sterowniki w sieci lokalnej i publikuje ich listę.
- **Aplikacja Android** łączy się z Mosquitto przez TLS, wyświetla wykryte
  sterowniki i wysyła polecenia zmiany stanu przekaźników oraz harmonogramu.

Sterowniki i provider muszą być w jednym segmencie LAN, ponieważ wykrywanie używa
rozgłoszeń UDP. Android może łączyć się spoza tej sieci. Harmonogram działa według
czasu CET/CEST i musi mieścić się w jednej dobie; szczegóły opisuje
[dokumentacja MQTT i harmonogramu](doc/pl/mqtt.md).

## Uruchomienie

Wykonaj instrukcje w tej kolejności:

1. Przygotuj [sieć i broker MQTT](doc/pl/network.md).
2. Skompiluj i skonfiguruj [provider urządzeń](RaspberryPi/aqua_topic_provider/README.pl.md).
3. Przygotuj i wgraj [firmware Pico W](TimerNTP/README.pl.md); ta instrukcja
   obejmuje przygotowanie komputera i [Credentials](Credentials/README.pl.md).
4. Skonfiguruj i uruchom [aplikację Android](Android/README.pl.md).

Firmware korzysta z [JaszczurHAL](https://github.com/jaszczurtd/JaszczurHAL)
i oficjalnego Pico SDK. Instrukcja firmware opisuje wymagany układ katalogów,
device tree, zadania VS Code, polecenia terminala oraz aktualizacje OTA.

## Licencja

Projekt jest udostępniony na [licencji MIT](LICENSE).
