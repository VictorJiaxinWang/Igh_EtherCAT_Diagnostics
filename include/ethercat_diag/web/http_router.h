#pragma once

#include "ethercat_diag/web/http_types.h"
#include "ethercat_diag/web/web_data_store.h"

#include <filesystem>

class HttpRouter
{
public:
    HttpRouter(
        std::filesystem::path data_directory,
        std::filesystem::path assets_directory);

    HttpResponse route(const HttpRequest& request) const;

private:
    WebDataStore data_store_;
    std::filesystem::path assets_directory_;
};
