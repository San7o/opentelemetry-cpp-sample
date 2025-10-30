// SPDX-License-Identifier: MIT
// Author:  Giovanni Santini
// Mail:    giovanni.santini@proton.me
// Github:  @San7o

#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/Environment.hpp"

#include <opentelemetry/exporters/ostream/span_exporter_factory.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <string>

//
// Opentelemetry setup
//

namespace ot = opentelemetry;

void InitTracer()
{
  // Create an exporter to the standard output
  auto exporter = ot::exporter::trace::OStreamSpanExporterFactory::Create();
  // The processor handles a span, whether to immediately forward it
  // (simple) or batch multiple spans
  auto processor =
    ot::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter));
  // The provider is a singleton that holds the global SDK configurations
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
    ot::sdk::trace::TracerProviderFactory::Create(std::move(processor));
  ot::trace::Provider::SetTracerProvider(provider);
}

void CleanupTracer()
{
  std::shared_ptr<opentelemetry::trace::TracerProvider> none;
  ot::trace::Provider::SetTracerProvider(none);
}

//
// Web server setup
//

class Handler : public oatpp::web::server::HttpRequestHandler
{
public:
  std::shared_ptr<OutgoingResponse>
  handle([[maybe_unused]]const std::shared_ptr<IncomingRequest>& request) override
  {
    // Get singleton tracer
    auto tracer =
      ot::trace::Provider::GetTracerProvider()->GetTracer("rolldice-tracer");
    // Start a span, which is a single operation within a Trace. Spans
    // can be also nested, and have a parent-child relationship
    auto span = tracer->StartSpan("RollDiceServer");
    
    int low = 1;
    int high = 7;
    int random = rand() % (high - low) + low;
    const std::string response = std::to_string(random);

    // Set attributes / events to trace
    span->SetAttribute("random-number", random);

    // End a span, which will be exported by the current exporter
    span->End();
    return ResponseFactory::createResponse(Status::CODE_200, response.c_str());
  }
};

void run()
{
  // Setup HTTP server
  auto router = oatpp::web::server::HttpRouter::createShared();
  router->route("GET", "/rolldice", std::make_shared<Handler>());
  auto connectionHandler =
    oatpp::web::server::HttpConnectionHandler::createShared(router);
  auto connectionProvider =
    oatpp::network::tcp::server::ConnectionProvider::createShared({
        "localhost", 8080, oatpp::network::Address::IP_4
      });
  
  oatpp::network::Server server(connectionProvider, connectionHandler);

  std::cout << "Server listening on port "
            << (const char*)connectionProvider->getProperty("port").getData()
            << "\n";
  server.run();
}

int main(void)
{
  oatpp::Environment::init();

  InitTracer();
  srand((int)time(0));
  run();
  
  oatpp::Environment::destroy();
  CleanupTracer();
  return 0;
}
