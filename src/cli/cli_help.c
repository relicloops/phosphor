#include "phosphor/cli.h"
#include "phosphor/color.h"
#include "phosphor/log.h"

#include <stdio.h>
#include <string.h>

/* ---- public API ---- */

int ph_cli_help(const ph_cli_config_t *config, const char *topic) {
    if (!config) return PH_ERR_INTERNAL;

    const char *b  = ph_color_for(stdout, PH_BOLD);
    const char *g  = ph_color_for(stdout, PH_BOLD PH_FG_GREEN);
    const char *c  = ph_color_for(stdout, PH_FG_CYAN);
    const char *y  = ph_color_for(stdout, PH_FG_YELLOW);
    const char *d  = ph_color_for(stdout, PH_DIM);
    const char *r  = ph_color_for(stdout, PH_RESET);

    if (!topic) {
        /* no topic: print general usage summary */
        printf("%susage:%s %s%s%s <command> [flags]\n\n",
               b, r, g, config->tool_name, r);
        printf("%scommands:%s\n", b, r);
        for (size_t i = 0; i < config->command_count; i++) {
            printf("  %s%-10s%s", c, config->commands[i].name, r);
            if (config->commands[i].description)
                printf("  %s", config->commands[i].description);
            printf("\n");
        }
        printf("\nrun '%s%s help <command>%s' for command-specific help.\n",
               d, config->tool_name, r);
        return 0;
    }

    /* find command by topic name */
    const ph_cmd_def_t *cmd = NULL;
    for (size_t i = 0; i < config->command_count; i++) {
        if (strcmp(config->commands[i].name, topic) == 0) {
            cmd = &config->commands[i];
            break;
        }
    }

    if (!cmd) {
        ph_log_error("unknown command: %s", topic);
        return PH_ERR_USAGE;
    }

    /* command header */
    printf("%s%s%s", g, cmd->name, r);
    if (cmd->description)
        printf(" -- %s", cmd->description);
    printf("\n\n");

    printf("%susage:%s %s%s %s%s",
           b, r, g, config->tool_name, cmd->name, r);
    if (cmd->accepts_positional) {
        printf(" %s[argument]%s", d, r);
    }
    if (cmd->spec_count > 0) {
        printf(" %s[flags]%s", d, r);
    }
    printf("\n");

    if (cmd->spec_count == 0) {
        printf("\n%sno flags.%s\n", d, r);
        return 0;
    }

    printf("\n%sflags:%s\n", b, r);
    for (size_t i = 0; i < cmd->spec_count; i++) {
        const ph_argspec_t *s = &cmd->specs[i];

        /* flag syntax */
        if (s->form == PH_FORM_TOGGLE) {
            printf("  %s--enable-%s%s / %s--disable-%s%s",
                   c, s->name, r, c, s->name, r);
        } else if (s->form == PH_FORM_ACTION) {
            printf("  %s--%s%s", c, s->name, r);
        } else {
            printf("  %s--%s%s=%s<%s>%s",
                   c, s->name, r, y, ph_arg_type_name(s->type), r);
        }

        /* description */
        if (s->description) {
            printf("\n      %s", s->description);
        }

        /* annotations on the same line as description */
        bool has_annotation = s->required || s->default_value ||
                               (s->choices && s->choice_count > 0);
        if (has_annotation) {
            printf("  %s(", d);
            bool first = true;
            if (s->required) {
                printf("required");
                first = false;
            }
            if (s->default_value) {
                if (!first) printf(", ");
                printf("default: %s", s->default_value);
                first = false;
            }
            if (s->choices && s->choice_count > 0) {
                if (!first) printf(", ");
                printf("choices: ");
                for (size_t ci = 0; ci < s->choice_count; ci++) {
                    if (ci > 0) printf("|");
                    printf("%s", s->choices[ci]);
                }
            }
            printf(")%s", r);
        }

        printf("\n");
    }

    return 0;
}
