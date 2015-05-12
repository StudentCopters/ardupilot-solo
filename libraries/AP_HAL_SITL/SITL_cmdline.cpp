/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#include <AP_HAL.h>

#if CONFIG_HAL_BOARD == HAL_BOARD_SITL

#include <AP_HAL_SITL.h>
#include "AP_HAL_SITL_Namespace.h"
#include "HAL_SITL_Class.h"
#include "UARTDriver.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <utility/getopt_cpp.h>

#include <SIM_Multicopter.h>
#include <SIM_Helicopter.h>
#include <SIM_Rover.h>
#include <SIM_CRRCSim.h>
#include <SIM_JSBSim.h>

extern const AP_HAL::HAL& hal;

using namespace HALSITL;

// catch floating point exceptions
static void _sig_fpe(int signum)
{
    fprintf(stderr, "ERROR: Floating point exception - aborting\n");
    abort();
}

void SITL_State::_usage(void)
{
    printf("Options:\n"
           "\t--home HOME        set home location (lat,lng,alt,yaw)\n"
           "\t--model MODEL      set simulation model\n"
           "\t--wipe             wipe eeprom and dataflash\n"
           "\t--rate RATE        set SITL framerate\n"
           "\t--console          use console instead of TCP ports\n"
           "\t--instance N       set instance of SITL (adds 10*instance to all port numbers)\n"
           "\t--speedup SPEEDUP  set simulation speedup\n"
        );
}

static const struct {
    const char *name;
    Aircraft *(*constructor)(const char *home_str, const char *frame_str);
} model_constructors[] = {
    { "+",         MultiCopter::create },
    { "quad",      MultiCopter::create },
    { "copter",    MultiCopter::create },
    { "x",         MultiCopter::create },
    { "hexa",      MultiCopter::create },
    { "octa",      MultiCopter::create },
    { "heli",      Helicopter::create },
    { "rover",     Rover::create },
    { "crrcsim",   CRRCSim::create },
    { "jsbsim",    JSBSim::create }
};

void SITL_State::_parse_command_line(int argc, char * const argv[])
{
    int opt;
    const char *home_str = NULL;
    const char *model_str = NULL;
    float speedup = 1.0f;

    signal(SIGFPE, _sig_fpe);
    // No-op SIGPIPE handler
    signal(SIGPIPE, SIG_IGN);

    setvbuf(stdout, (char *)0, _IONBF, 0);
    setvbuf(stderr, (char *)0, _IONBF, 0);

    _synthetic_clock_mode = false;
    _base_port = 5760;
    _rcout_port = 5502;
    _simin_port = 5501;
    _fdm_address = "127.0.0.1";
    _client_address = NULL;
    _instance = 0;

    enum long_options {
        CMDLINE_CLIENT=0
    };

    const struct GetOptLong::option options[] = {
        {"help",            false,  0, 'h'},
        {"wipe",            false,  0, 'w'},
        {"speedup",         true,   0, 's'},
        {"rate",            true,   0, 'r'},
        {"console",         false,  0, 'C'},
        {"instance",        true,   0, 'I'},
        {"param",           true,   0, 'P'},
        {"synthetic-clock", false,  0, 'S'},
        {"home",            true,   0, 'O'},
        {"model",           true,   0, 'M'},
        {"client",          true,   0, CMDLINE_CLIENT},
        {0, false, 0, 0}
    };

    GetOptLong gopt(argc, argv, "hws:r:CI:P:SO:M:F:",
                    options);

    while ((opt = gopt.getoption()) != -1) {
        switch (opt) {
        case 'w':
            AP_Param::erase_all();
            unlink("dataflash.bin");
            break;
        case 'r':
            _framerate = (unsigned)atoi(gopt.optarg);
            break;
        case 'C':
            HALSITL::SITLUARTDriver::_console = true;
            break;
        case 'I': {
            _instance = atoi(gopt.optarg);
            _base_port  += _instance * 10;
            _rcout_port += _instance * 10;
            _simin_port += _instance * 10;
        }
        break;
        case 'P':
            _set_param_default(gopt.optarg);
            break;
        case 'S':
            _synthetic_clock_mode = true;
            break;
        case 'O':
            home_str = gopt.optarg;
            break;
        case 'M':
            model_str = gopt.optarg;
            break;
        case 's':
            speedup = atof(gopt.optarg);
            break;
        case 'F':
            _fdm_address = gopt.optarg;
            break;
        case CMDLINE_CLIENT:
            _client_address = gopt.optarg;
            break;
        default:
            _usage();
            exit(1);
        }
    }

    if (model_str && home_str) {
        for (uint8_t i=0; i<sizeof(model_constructors)/sizeof(model_constructors[0]); i++) {
            if (strncmp(model_constructors[i].name, model_str, strlen(model_constructors[i].name)) == 0) {
                sitl_model = model_constructors[i].constructor(home_str, model_str);
                sitl_model->set_speedup(speedup);
                sitl_model->set_instance(_instance);
                _synthetic_clock_mode = true;
                printf("Started model %s at %s at speed %.1f\n", model_str, home_str, speedup);
                break;
            }
        }
    }

    fprintf(stdout, "Starting sketch '%s'\n", SKETCH);

    if (strcmp(SKETCH, "ArduCopter") == 0) {
        _vehicle = ArduCopter;
        if (_framerate == 0) {
            _framerate = 200;
        }
    } else if (strcmp(SKETCH, "APMrover2") == 0) {
        _vehicle = APMrover2;
        if (_framerate == 0) {
            _framerate = 50;
        }
        // set right default throttle for rover (allowing for reverse)
        pwm_input[2] = 1500;
    } else {
        _vehicle = ArduPlane;
        if (_framerate == 0) {
            _framerate = 50;
        }
    }

    _sitl_setup();
}

#endif