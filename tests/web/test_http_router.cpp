#include "ethercat_diag/web/http_router.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace
{

class Fixture
{
public:
    Fixture()
        : root(
              std::filesystem::temp_directory_path() /
              ("ethercat-http-router-" + std::to_string(::getpid()))),
          data(root / "data"),
          assets(root / "assets")
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(data);
        std::filesystem::create_directories(assets);
        write(data / "latest_status.json", "{\"status\":\"FAULT\"}\n");
        write(
            data / "events.jsonl",
            "{\"id\":1}\n{\"id\":2}\n{\"id\":3}\n");
        write(assets / "index.html", "<main>dashboard</main>\n");
        write(assets / "styles.css", "body{}\n");
        write(assets / "app.js", "'use strict';\n");
    }

    ~Fixture()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    static void write(
        const std::filesystem::path& path,
        const std::string& contents)
    {
        std::ofstream output(path);
        output << contents;
    }

    std::filesystem::path root;
    std::filesystem::path data;
    std::filesystem::path assets;
};

void testHealthAndDiagnosticApis()
{
    Fixture fixture;
    HttpRouter router(fixture.data, fixture.assets);

    const HttpResponse health = router.route({"GET", "/healthz"});
    assert(health.status == 200);
    assert(health.content_type == "application/json; charset=utf-8");
    assert(health.body == "{\"status\":\"ok\"}\n");

    const HttpResponse status = router.route({"GET", "/api/status"});
    assert(status.status == 200);
    assert(status.body == "{\"status\":\"FAULT\"}\n");

    const HttpResponse events =
        router.route({"GET", "/api/events?limit=2"});
    assert(events.status == 200);
    assert(events.body == "[{\"id\":2},{\"id\":3}]\n");
}

void testEventLimitValidation()
{
    Fixture fixture;
    HttpRouter router(fixture.data, fixture.assets);

    assert(router.route({"GET", "/api/events"}).status == 200);
    assert(router.route({"GET", "/api/events?limit=1"}).status == 200);
    assert(router.route({"GET", "/api/events?limit=200"}).status == 200);
    assert(router.route({"GET", "/api/events?limit=0"}).status == 400);
    assert(router.route({"GET", "/api/events?limit=201"}).status == 400);
    assert(router.route({"GET", "/api/events?limit=abc"}).status == 400);
    assert(router.route({"GET", "/api/events?other=1"}).status == 400);
}

void testStaticWhitelistAndMethodRestriction()
{
    Fixture fixture;
    HttpRouter router(fixture.data, fixture.assets);

    const HttpResponse index = router.route({"GET", "/"});
    assert(index.status == 200);
    assert(index.content_type == "text/html; charset=utf-8");
    assert(index.body == "<main>dashboard</main>\n");

    assert(router.route({"GET", "/styles.css"}).content_type ==
           "text/css; charset=utf-8");
    assert(router.route({"GET", "/app.js"}).content_type ==
           "application/javascript; charset=utf-8");
    assert(router.route({"GET", "/../latest_status.json"}).status == 404);
    assert(router.route({"GET", "/%2e%2e/latest_status.json"}).status == 404);
    assert(router.route({"POST", "/api/status"}).status == 405);
}

void testUnavailableStatusAndWireFormat()
{
    Fixture fixture;
    std::filesystem::remove(fixture.data / "latest_status.json");
    HttpRouter router(fixture.data, fixture.assets);

    const HttpResponse unavailable = router.route({"GET", "/api/status"});
    assert(unavailable.status == 503);
    assert(unavailable.body ==
           "{\"error\":\"diagnostic status is unavailable\"}\n");

    const std::string wire = serializeHttpResponse(
        HttpResponse{200, "OK", "text/plain; charset=utf-8", "abc"});
    assert(wire ==
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/plain; charset=utf-8\r\n"
           "Content-Length: 3\r\n"
           "Cache-Control: no-store\r\n"
           "Connection: close\r\n"
           "\r\n"
           "abc");
}

}

int main()
{
    testHealthAndDiagnosticApis();
    testEventLimitValidation();
    testStaticWhitelistAndMethodRestriction();
    testUnavailableStatusAndWireFormat();
    return 0;
}
