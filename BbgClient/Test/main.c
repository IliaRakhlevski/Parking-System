#include "bbg_client.h"

#define BBG_CLIENT_CONFIG_FILE_PATH "Config/bbg_client.conf"

int main(void)
{
    return bbg_client_run(BBG_CLIENT_CONFIG_FILE_PATH);
}
