#include "routing/routing_quality/api/api.hpp"

#include "coding/write_to_sink.hpp"

#include "geometry/mercator.hpp"

#include <string>

namespace routing_quality
{
namespace api
{

// static
void Params::Dump(Params const & route, FileWriter & writer)
{
  auto const start = mercator::ToLatLon(route.m_waypoints.GetPointFrom());
  auto const finish = mercator::ToLatLon(route.m_waypoints.GetPointTo());

  WriteToSink(writer, static_cast<int>(route.m_type));

  writer.Write(&start.m_lat, sizeof(start.m_lat));
  writer.Write(&start.m_lon, sizeof(start.m_lon));
  writer.Write(&finish.m_lat, sizeof(finish.m_lat));
  writer.Write(&finish.m_lon, sizeof(finish.m_lon));
}

// static
Params Params::Load(ReaderSource<FileReader> & src)
{
  Params params;

  params.m_type = static_cast<VehicleType>(ReadPrimitiveFromSource<int>(src));

  ms::LatLon start;
  ms::LatLon finish;

  start.m_lat = ReadPrimitiveFromSource<double>(src);
  start.m_lon = ReadPrimitiveFromSource<double>(src);
  finish.m_lat = ReadPrimitiveFromSource<double>(src);
  finish.m_lon = ReadPrimitiveFromSource<double>(src);

  auto const startPoint = mercator::FromLatLon(start);
  auto const finishPoint = mercator::FromLatLon(finish);

  params.m_waypoints = routing::Checkpoints(startPoint, finishPoint);

  return params;
}

// static
void Route::Dump(Route const & route, FileWriter & writer)
{
  writer.Write(&route.m_eta, sizeof(route.m_eta));
  writer.Write(&route.m_distance, sizeof(route.m_distance));

  WriteToSink(writer, route.m_waypoints.size());
  for (auto const & latlon : route.m_waypoints)
  {
    writer.Write(&latlon.m_lat, sizeof(latlon.m_lat));
    writer.Write(&latlon.m_lon, sizeof(latlon.m_lon));
  }
}

// static
Route Route::Load(ReaderSource<FileReader> & src)
{
  Route route;

  route.m_eta = ReadPrimitiveFromSource<double>(src);
  route.m_distance = ReadPrimitiveFromSource<double>(src);

  auto const n = ReadPrimitiveFromSource<size_t>(src);
  route.m_waypoints.resize(n);
  ms::LatLon latlon;
  for (size_t i = 0; i < n; ++i)
  {
    latlon.m_lat = ReadPrimitiveFromSource<double>(src);
    latlon.m_lon = ReadPrimitiveFromSource<double>(src);
    route.m_waypoints[i] = latlon;
  }

  return route;
}

// static
std::string const Response::kDumpExtension = ".api.dump";

// static
void Response::Dump(std::string const & filepath, Response const & response)
{
  FileWriter writer(filepath);

  WriteToSink(writer, static_cast<int>(response.m_code));

  Params::Dump(response.m_params, writer);

  WriteToSink(writer, response.m_routes.size());
  for (auto const & route : response.m_routes)
    Route::Dump(route, writer);
}

// static
Response Response::Load(std::string const & filepath)
{
  FileReader reader(filepath);
  ReaderSource<FileReader> src(reader);

  Response response;
  response.m_code = static_cast<ResultCode>(ReadPrimitiveFromSource<int>(src));
  response.m_params = Params::Load(src);

  auto const n = ReadPrimitiveFromSource<size_t>(src);
  response.m_routes.resize(n);
  for (size_t i = 0; i < n; ++i)
    response.m_routes[i] = Route::Load(src);

  return response;
}

routing::VehicleType Response::GetVehicleType() const
{
  switch (m_params.m_type)
  {
  case VehicleType::Car: return routing::VehicleType::Car;
  }
  UNREACHABLE();
}


RoutingApi::RoutingApi(std::string name, std::string token, uint32_t maxRPS)
  : m_apiName(std::move(name))
  , m_accessToken(std::move(token))
  , m_maxRPS(maxRPS) {}

Response RoutingApi::CalculateRoute(Params const &, int32_t) const
{
  return {};
}

uint32_t RoutingApi::GetMaxRPS() const
{
  return m_maxRPS;
}

std::string const & RoutingApi::GetApiName() const
{
  return m_apiName;
}

std::string const & RoutingApi::GetAccessToken() const
{
  return m_accessToken;
}
}  // namespace api
}  // namespace routing_quality
