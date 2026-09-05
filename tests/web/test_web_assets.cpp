#include "ethercat_diag/web/http_router.h"

#include <cassert>
#include <filesystem>
#include <string>

int main()
{
    HttpRouter router(
        std::filesystem::path(TEST_WEB_ASSET_DIR) / "missing-data",
        TEST_WEB_ASSET_DIR);

    const HttpResponse page = router.route({"GET", "/"});
    assert(page.status == 200);
    assert(page.content_type == "text/html; charset=utf-8");

    const std::string required_regions[]{
        "id=\"overall-status\"",
        "id=\"topology\"",
        "id=\"diagnosis\"",
        "id=\"recovery-steps\"",
        "id=\"event-timeline\""};
    for (const std::string& region : required_regions)
    {
        assert(page.body.find(region) != std::string::npos);
    }

    const HttpResponse styles = router.route({"GET", "/styles.css"});
    assert(styles.status == 200);
    assert(styles.content_type == "text/css; charset=utf-8");
    assert(!styles.body.empty());

    const HttpResponse script = router.route({"GET", "/app.js"});
    assert(script.status == 200);
    assert(script.content_type == "application/javascript; charset=utf-8");
    assert(!script.body.empty());
    return 0;
}
