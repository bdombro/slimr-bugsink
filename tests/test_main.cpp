#define SLIMR_TEST_MAIN
#include "bugsink.hpp"
#include "test.hpp"
#include <mutex>
#include <sstream>
#include <thread>

using namespace slimr;

TEST_SUITE("slimr::bugsink") {
    TEST_CASE("DSN parsing") {
        auto parsed = BugsinkReporter::parseDsn("https://8973845e128c4204ba7a7e02eaba6a17@bugsink.btek.cc/4");
        CHECK(parsed.valid == true);
        CHECK(parsed.protocol == "https");
        CHECK(parsed.publicKey == "8973845e128c4204ba7a7e02eaba6a17");
        CHECK(parsed.host == "bugsink.btek.cc");
        CHECK(parsed.projectId == "4");

        unsetenv("TEST_UNSET_BUGSINK_DSN");
        BugsinkReporter reporter("https://key123@example.com/99", "TEST_UNSET_BUGSINK_DSN");
        CHECK(reporter.envelopeUrl() == "https://example.com/api/99/envelope/?sentry_key=key123&sentry_version=7");
    }

    TEST_CASE("Envelope NDJSON formatting") {
        BugsinkReporter reporter("https://pub@sentry.local/1");
        Json payload = Json::object_t{};
        payload["message"] = "test event";

        std::string env = reporter.formatEnvelope(payload, "event123", "2026-11-10T12:00:00Z");

        // Verify it contains exactly 3 newline-separated JSON objects
        std::vector<std::string> lines;
        std::stringstream ss(env);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty())
                lines.push_back(line);
        }

        CHECK(lines.size() == 3);
        CHECK(lines[0].find("\"event_id\":\"event123\"") != std::string::npos);
        CHECK(lines[0].find("\"sent_at\":\"2026-11-10T12:00:00Z\"") != std::string::npos);
        CHECK(lines[1] == "{\"type\":\"event\",\"content_type\":\"application/json\"}");
        CHECK(lines[2].find("\"message\":\"test event\"") != std::string::npos);
    }

    TEST_CASE("Environment override disables on empty string") {
        setenv("TEST_BUGSINK_DSN", "", 1);
        BugsinkReporter reporter("https://pub@sentry.local/1", "TEST_BUGSINK_DSN");
        CHECK(reporter.effectiveDsn() == "");
        CHECK(reporter.envelopeUrl() == "");
        unsetenv("TEST_BUGSINK_DSN");
    }

    TEST_CASE("Release parameter is included when non-empty and omitted when empty") {
        BugsinkReporter withRelease("https://pub@sentry.local/1", "BUGSINK_DSN", "app@1.2.3");
        CHECK(withRelease.release() == "app@1.2.3");
        auto errPayload = withRelease.buildErrorPayload(std::runtime_error("boom"), "ev1", "2026-11-15T00:00:00Z");
        CHECK(errPayload["release"].get<std::string>() == "app@1.2.3");
        auto msgPayload = withRelease.buildMessagePayload("hi", "info", "ev2", "2026-11-15T00:00:00Z");
        CHECK(msgPayload["release"].get<std::string>() == "app@1.2.3");

        BugsinkReporter withoutRelease("https://pub@sentry.local/1");
        CHECK(withoutRelease.release().empty());
        auto errNoRel = withoutRelease.buildErrorPayload(std::runtime_error("boom"), "ev3", "2026-11-15T00:00:00Z");
        CHECK(!errNoRel.contains("release"));
        auto msgNoRel = withoutRelease.buildMessagePayload("hi", "info", "ev4", "2026-11-15T00:00:00Z");
        CHECK(!msgNoRel.contains("release"));
    }

    TEST_CASE("reportError and reportMessage POST envelope and swallow failures") {
        httplib::Server svr;
        std::mutex mu;
        std::vector<std::string> contentTypes;
        std::vector<std::string> bodies;
        std::vector<std::string> keys;

        svr.Post("/api/42/envelope/", [&](const httplib::Request &req, httplib::Response &res) {
            std::lock_guard lock(mu);
            contentTypes.push_back(req.get_header_value("Content-Type"));
            bodies.push_back(req.body);
            keys.push_back(req.get_param_value("sentry_key"));
            res.status = 200;
        });

        int port = svr.bind_to_any_port("127.0.0.1");
        REQUIRE(port > 0);
        std::thread serverThread([&]() { svr.listen_after_bind(); });
        svr.wait_until_ready();

        std::string dsn = "http://key42@127.0.0.1:" + std::to_string(port) + "/42";
        BugsinkReporter reporter(dsn, "TEST_UNSET_BUGSINK_DSN");

        CHECK(reporter.reportError(std::runtime_error("boom")) == true);
        CHECK(reporter.reportMessage("warn msg", "warning") == true);

        svr.stop();
        serverThread.join();

        {
            std::lock_guard lock(mu);
            REQUIRE(bodies.size() == 2);
            CHECK(contentTypes[0] == "text/plain;charset=UTF-8");
            CHECK(contentTypes[1] == "text/plain;charset=UTF-8");
            CHECK(keys[0] == "key42");
            CHECK(keys[1] == "key42");
            CHECK(bodies[0].find("boom") != std::string::npos);
            CHECK(bodies[1].find("warn msg") != std::string::npos);
        }

        // Unreachable server returns false without throwing
        CHECK(reporter.reportError(std::runtime_error("unreachable")) == false);
        CHECK(reporter.reportMessage("unreachable") == false);
    }
}
