/*
 * Copyright (c) 2020-2022, Thanos Siopoudis
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "ps1_gameid.h"

static void rolfl(char *str, const char *toRemove) {
    if (NULL == (str = strstr(str, toRemove))) {
        // no match.
        return;
    }

    // str points to toRemove in str now.
    const size_t remLen = strlen(toRemove);
    char *copyEnd;
    char *copyFrom = str + remLen;
    while (NULL != (copyEnd = strstr(copyFrom, toRemove))) {
        memmove(str, copyFrom, copyEnd - copyFrom);
        str += copyEnd - copyFrom;
        copyFrom = copyEnd + remLen;
    }
    memmove(str, copyFrom, 1 + strlen(copyFrom));
}

static char* replace_char(char* str, char find, char replace) {
    char *current_pos = strchr(str,find);
    while (current_pos) {
        *current_pos = replace;
        current_pos = strchr(current_pos,find);
    }
    return str;
}

void ps1_gid_sanitize(char *gameID) {
    rolfl((char *)gameID, "cdrom:");
    rolfl((char *)gameID, "\\DSC\\");
    rolfl((char *)gameID, "\\EXE\\");
    rolfl((char *)gameID, "\\exe\\");
    rolfl((char *)gameID, "\\TEKKEN3\\");
    rolfl((char *)gameID, "\\MGS\\");
    rolfl((char *)gameID, "/");
    rolfl((char *)gameID, "\\");
    rolfl((char *)gameID, ".");
    replace_char((char *)gameID, '_', '-');
    replace_char((char *)gameID, ';', '\0');
}

