#include <linphone/core.h>
#include <signal.h>
#include <bctoolbox/defs.h>
#include <mediastreamer2/mscommon.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <stdbool.h>

static LinphoneCore *lc = NULL;
static volatile int running = 1;

void signal_handler(int signum) { 
    running = 0; 
    if (lc) {
        linphone_core_stop(lc);
    }
}

static void call_state_changed(LinphoneCore *lc, LinphoneCall *call, 
                              LinphoneCallState cstate, const char *msg) {
    printf("Call state changed to: %s\n", linphone_call_state_to_string(cstate));
    
    if (cstate == LinphoneCallStateIncomingReceived) {
        const LinphoneAddress *remote_addr = linphone_call_get_remote_address(call);
        if (remote_addr) {
            char *addr_str = linphone_address_as_string(remote_addr);
            printf("Incoming call from %s\n", addr_str);
            ms_free(addr_str);
            
            // Akceptuj połączenie
            LinphoneCallParams *params = linphone_core_create_call_params(lc, call);
            if (params) {
                linphone_call_params_enable_video(params, TRUE);
                linphone_call_params_enable_audio(params, TRUE);

                linphone_call_params_set_video_direction(params, LinphoneMediaDirectionSendOnly);

                linphone_call_params_set_audio_bandwidth_limit(params, 24);
              
                // Automatyczna odpowiedź
                if (linphone_call_accept_with_params(call, params) != 0) {
                    printf("Failed to accept call\n");
                } else {
                    printf("Call accepted\n");
                }
                linphone_call_params_unref(params);
            }
        }
    }
}

/* Pomocniczo – zwraca pierwsze urządzenie zawierające `needle`   */
static const char *find_device(bctbx_list_t *list, const char *needle)
{
    for (bctbx_list_t *it = list; it; it = it->next)
        if (strstr((const char *)it->data, needle))
            return (const char *)it->data;
    return NULL;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);

    // Pobierz ścieżkę do katalogu domowego użytkownika
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    
    // Utwórz katalog dla bazy danych
    char dbpath[256];
    snprintf(dbpath, sizeof(dbpath), "%s/.local/share/linphone", homedir);
    mkdir(dbpath, 0777);
    
    printf("Using database path: %s\n", dbpath);

    LinphoneFactory *factory = linphone_factory_get();
    if (!factory) {
        fprintf(stderr, "Failed to get Linphone factory\n");
        return 1;
    }
    
    LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(factory);
    if (!cbs) {
        fprintf(stderr, "Failed to create core callbacks\n");
        return 1;
    }
    
    linphone_core_cbs_set_call_state_changed(cbs, call_state_changed);
    
    // Konfiguracja
    LinphoneConfig *config = linphone_factory_create_config(factory, NULL);
    if (!config) {
        fprintf(stderr, "Failed to create config\n");
        return 1;
    }
    
    linphone_config_set_string(config, "sip", "contact", "sip:pi@10.8.0.1");
    linphone_config_set_int(config, "sip", "keepalive_period", 30000);
    linphone_config_set_int(config, "sip", "inc_timeout", 600);
    
    linphone_config_set_int(config, "sound", "ec", 0); // Wyłącz echo cancellation
    linphone_config_set_int(config, "sound", "capture_rate", 11000);
    linphone_config_set_int(config, "sound", "playback_rate", 11000);

    linphone_config_set_int(config, "net", "force_ice_disablement", 0);
    linphone_config_set_int(config, "net", "mtu", 1300);
    
    // Utwórz instancję LinphoneCore
    lc = linphone_factory_create_core_with_config_3(factory, config, NULL);
    if (!lc) {
        fprintf(stderr, "Failed to create Linphone core\n");
        return 1;
    }
    linphone_core_enable_ipv6(lc, FALSE);

    linphone_core_add_callbacks(lc, cbs);

    linphone_core_set_user_agent(lc, "Raspberry Pi linphone client", "1.0");
    
    // Ustawienia SIP
    LinphoneAuthInfo *auth = linphone_auth_info_new("pi", NULL, "secret", NULL, NULL, NULL);
    if (auth) {
        linphone_core_add_auth_info(lc, auth);
    }
    
    LinphoneAddress *addr = linphone_address_new("sip:pi@0.0.0.0");
    if (addr) {
        char *contact = linphone_address_as_string(addr);
        if (contact) {
            linphone_core_set_primary_contact(lc, contact);
            ms_free(contact);
        }
        linphone_address_unref(addr);
    }

    /* ---------- AUDIO ----------- */
    printf("\nAvailable audio playback devices:\n");
    bctbx_list_t *audio = linphone_core_get_sound_devices_list(lc);
    int idx = 0;
    for (bctbx_list_t *it = audio; it; it = it->next)
        printf("%2d) %s\n", idx++, (const char *)it->data);

    /* Wybieramy po fragmencie nazwy */
    const char *cap  = find_device(audio, "Usb Audio Device");
    const char *play = find_device(audio, "Wbudowany");

    if (cap  && linphone_core_set_capture_device (lc, cap ) == 0)
        printf("Capture  device set to  : %s\n", cap );
    else  fprintf(stderr, "► Nie znalazłem/mogę ustawić mikrofonu!\n");

    if (play && linphone_core_set_playback_device(lc, play) == 0)
        printf("Playback device set to  : %s\n", play);
    else  fprintf(stderr, "► Nie znalazłem/mogę ustawić głośnika!\n");

    bctbx_list_free(audio);

    /* ---------- VIDEO ----------- */
    printf("\nAvailable video devices:\n");
    bctbx_list_t *video = linphone_core_get_video_devices_list(lc);
    idx = 0;
    for (bctbx_list_t *it = video; it; it = it->next)
        printf("%2d) %s\n", idx++, (const char *)it->data);

    const char *cam = find_device(video, "/dev/video0");          /* "V4L2: /dev/video0" */
    if (cam && linphone_core_set_video_device(lc, cam) == 0)
        printf("Video    device set to  : %s\n", cam);
    else  fprintf(stderr, "► Nie znalazłem/mogę ustawić kamery!\n");

    bctbx_list_free(video);

    /* ---- weryfikacja ---- */
    printf("\nAktualne ustawienia:\n");
    printf("  capture : %s\n", linphone_core_get_capture_device (lc));
    printf("  playback: %s\n", linphone_core_get_playback_device(lc));
    printf("  video   : %s\n", linphone_core_get_video_device   (lc));    

    // Ustawianie kodeków
    const bctbx_list_t *video_codecs = linphone_core_get_video_payload_types(lc);
    for (const bctbx_list_t *elem = video_codecs; elem != NULL; elem = bctbx_list_next(elem)) {
        LinphonePayloadType *pt = (LinphonePayloadType *)elem->data;
        if (pt && strcmp(linphone_payload_type_get_mime_type(pt), "H264") == 0) {
            linphone_payload_type_enable(pt, TRUE);
            printf("Enabled H264 video codec\n");
        }
    }

    const bctbx_list_t *audio_codecs = linphone_core_get_audio_payload_types(lc);
    for (const bctbx_list_t *e = audio_codecs; e; e = e->next) {
        LinphonePayloadType *pt = e->data;
        const char *mime = linphone_payload_type_get_mime_type(pt);

        if (strcmp(mime, "opus") == 0) {
            linphone_payload_type_enable(pt, TRUE);
            /* ogranicz bitrate w samym kodeku (Opus) */
            linphone_payload_type_set_send_fmtp(pt, "maxaveragebitrate=20000; useinbandfec=1");
        } else if (strcmp(mime, "speex") == 0 &&
                   linphone_payload_type_get_clock_rate(pt) == 8000) {
            linphone_payload_type_enable(pt, TRUE);          /* Speex/8k ~15 kbit/s */
        } else {
            linphone_payload_type_enable(pt, FALSE);         /* wyłącz resztę (PCMA/PCMU) */
        }
    }

    linphone_core_enable_forced_ice_relay(lc, TRUE);
    linphone_core_set_nortp_timeout(lc, 600);
    linphone_core_set_video_port_range(lc, 10001, 11000);

    linphone_core_enable_adaptive_rate_control(lc, false);
    linphone_core_set_download_bandwidth(lc, 400);
    linphone_core_set_upload_bandwidth(lc, 400);

    // Uruchomienie core
    if (linphone_core_start(lc)) {
        fprintf(stderr, "Failed to start Linphone core\n");
        return 1;
    }

    printf("\nLinphone core started successfully!\n");
    printf("Waiting for incoming calls...\n");

    // Główna pętla
    while(running) {
        linphone_core_iterate(lc);
        ms_usleep(50000);
    }

    // Sprzątanie
    linphone_core_unref(lc);
    return 0;
}

