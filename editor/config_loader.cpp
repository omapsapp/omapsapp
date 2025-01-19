#include "editor/config_loader.hpp"
#include "editor/editor_config.hpp"

#include "platform/platform.hpp"

#include "coding/internal/file_data.hpp"
#include "coding/reader.hpp"

#include <stdexcept>

#include <pugixml.hpp>

namespace editor
{
using std::string;

namespace
{

constexpr char kConfigFileName[] = "editor.config";

}  // namespace

void Waiter::Interrupt()
{
  {
    std::lock_guard lock(m_mutex);
    m_interrupted = true;
  }

  m_event.notify_all();
}

ConfigLoader::ConfigLoader(std::atomic<std::shared_ptr<EditorConfig>> & config) : m_config(config)
{
  pugi::xml_document doc;
  LoadFromLocal(doc);
  ResetConfig(doc);
}

ConfigLoader::~ConfigLoader()
{
  m_waiter.Interrupt();
}

void ConfigLoader::ResetConfig(pugi::xml_document const & doc)
{
  auto config = std::make_shared<EditorConfig>();
  config->SetConfig(doc);
  m_config.store(config);
}

// static
void ConfigLoader::LoadFromLocal(pugi::xml_document & doc)
{
  string content;
  std::unique_ptr<ModelReader> reader;

  try
  {
    // Get config file from WritableDir first.
    reader = GetPlatform().GetReader(kConfigFileName, "wr");
  }
  catch (RootException const & ex)
  {
    LOG(LERROR, (ex.Msg()));
    return;
  }

  if (reader)
    reader->ReadAsString(content);

  auto const result = doc.load_buffer(content.data(), content.size());
  if (!result)
  {
    LOG(LERROR, (kConfigFileName, "can not be loaded:", result.description(), "error offset:", result.offset));
    doc.reset();
  }
}

}  // namespace editor
