#include "phosphor/cli.h"
#include "phosphor/color.h"
#include "phosphor/commands.h"
#include "phosphor/signal.h"
#include "phosphor/log.h"

#include <stdlib.h>

int main(int argc, char *argv[]) {
    ph_color_init(PH_COLOR_AUTO);
    ph_signal_install();

    if (argc < 2) {
        ph_log_error("no command specified (try: phosphor help)");
        return PH_ERR_USAGE;
    }

    /* lexer: argv -> token stream */
    ph_token_stream_t tokens = {0};
    ph_error_t *err = NULL;

    if (ph_lexer_tokenize(argc, (const char *const *)argv,
                           &tokens, &err) != PH_OK) {
        if (err) {
            ph_log_error("%s", err->message);
            ph_error_destroy(err);
        }
        return PH_ERR_USAGE;
    }

    /* parser: tokens -> parsed args */
    ph_parsed_args_t args = {0};

    if (ph_parser_parse(&phosphor_cli_config, &tokens,
                         &args, &err) != PH_OK) {
        if (err) {
            ph_log_error("%s", err->message);
            ph_error_destroy(err);
        }
        ph_token_stream_destroy(&tokens);
        return PH_ERR_USAGE;
    }

    /* defaults: inject argspec defaults for absent valued flags */
    if (ph_args_apply_defaults(&phosphor_cli_config, &args, &err) != PH_OK) {
        if (err) {
            ph_log_error("%s", err->message);
            ph_error_destroy(err);
        }
        ph_parsed_args_destroy(&args);
        ph_token_stream_destroy(&tokens);
        return PH_ERR_INTERNAL;
    }

    /* validator: semantic checks */
    if (ph_validate(&phosphor_cli_config, &args, &err) != PH_OK) {
        if (err) {
            ph_log_error("%s", err->message);
            ph_error_destroy(err);
        }
        ph_parsed_args_destroy(&args);
        ph_token_stream_destroy(&tokens);
        return PH_ERR_USAGE;
    }

    /* signal check before dispatch */
    if (ph_signal_interrupted()) {
        ph_parsed_args_destroy(&args);
        ph_token_stream_destroy(&tokens);
        return PH_ERR_SIGNAL;
    }

    /* dispatch */
    int rc = ph_cli_dispatch(&phosphor_cli_config, &args);

    /* cleanup */
    ph_parsed_args_destroy(&args);
    ph_token_stream_destroy(&tokens);

    return rc;
}
