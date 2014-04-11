#include <xmpp/xmpp_parser.hpp>
#include <xmpp/xmpp_stanza.hpp>

#include <logger/logger.hpp>

/**
 * Expat handlers. Called by the Expat library, never by ourself.
 * They just forward the call to the XmppParser corresponding methods.
 */

static void start_element_handler(void* user_data, const XML_Char* name, const XML_Char** atts)
{
  static_cast<XmppParser*>(user_data)->start_element(name, atts);
}

static void end_element_handler(void* user_data, const XML_Char* name)
{
  static_cast<XmppParser*>(user_data)->end_element(name);
}

static void character_data_handler(void *user_data, const XML_Char *s, int len)
{
  static_cast<XmppParser*>(user_data)->char_data(s, len);
}

/**
 * XmppParser class
 */

XmppParser::XmppParser():
  level(0),
  current_node(nullptr)
{
  this->init_xml_parser();
}

void XmppParser::init_xml_parser()
{
  // Create the expat parser
  this->parser = XML_ParserCreateNS("UTF-8", ':');
  XML_SetUserData(this->parser, static_cast<void*>(this));

  // Install Expat handlers
  XML_SetElementHandler(this->parser, &start_element_handler, &end_element_handler);
  XML_SetCharacterDataHandler(this->parser, &character_data_handler);
}

XmppParser::~XmppParser()
{
  if (this->current_node)
    delete this->current_node;
  XML_ParserFree(this->parser);
}

int XmppParser::feed(const char* data, const int len, const bool is_final)
{
  int res = XML_Parse(this->parser, data, len, is_final);
  if (res == XML_STATUS_ERROR &&
      (XML_GetErrorCode(this->parser) != XML_ERROR_FINISHED))
    log_error("Xml_Parse encountered an error: " <<
              XML_ErrorString(XML_GetErrorCode(this->parser)))
  return res;
}

int XmppParser::parse(const int len, const bool is_final)
{
  int res = XML_ParseBuffer(this->parser, len, is_final);
  if (res == XML_STATUS_ERROR)
    log_error("Xml_Parsebuffer encountered an error: " <<
              XML_ErrorString(XML_GetErrorCode(this->parser)));
  return res;
}

void XmppParser::reset()
{
  XML_ParserFree(this->parser);
  this->init_xml_parser();
  if (this->current_node)
    delete this->current_node;
  this->current_node = nullptr;
  this->level = 0;
}

void* XmppParser::get_buffer(const size_t size) const
{
  return XML_GetBuffer(this->parser, static_cast<int>(size));
}

void XmppParser::start_element(const XML_Char* name, const XML_Char** attribute)
{
  level++;

  XmlNode* new_node = new XmlNode(name, this->current_node);
  if (this->current_node)
    this->current_node->add_child(new_node);
  this->current_node = new_node;
  for (size_t i = 0; attribute[i]; i += 2)
    this->current_node->set_attribute(attribute[i], attribute[i+1]);
  if (this->level == 1)
    this->stream_open_event(*this->current_node);
}

void XmppParser::end_element(const XML_Char* name)
{
  (void)name;
  level--;
  this->current_node->close();
  if (level == 1)
    {
      this->stanza_event(*this->current_node);
    }
  if (level == 0)
    {
      this->stream_close_event(*this->current_node);
      delete this->current_node;
      this->current_node = nullptr;
    }
  else
    this->current_node = this->current_node->get_parent();
  if (level == 1)
    this->current_node->delete_all_children();
}

void XmppParser::char_data(const XML_Char* data, int len)
{
  if (this->current_node->has_children())
    this->current_node->get_last_child()->add_to_tail(std::string(data, len));
  else
    this->current_node->add_to_inner(std::string(data, len));
}

void XmppParser::stanza_event(const Stanza& stanza) const
{
  for (const auto& callback: this->stanza_callbacks)
    {
      try {
        callback(stanza);
      } catch (const std::exception& e) {
        log_debug("Unhandled exception: " << e.what());
      }
    }
}

void XmppParser::stream_open_event(const XmlNode& node) const
{
  for (const auto& callback: this->stream_open_callbacks)
    callback(node);
}

void XmppParser::stream_close_event(const XmlNode& node) const
{
  for (const auto& callback: this->stream_close_callbacks)
    callback(node);
}

void XmppParser::add_stanza_callback(std::function<void(const Stanza&)>&& callback)
{
  this->stanza_callbacks.emplace_back(std::move(callback));
}

void XmppParser::add_stream_open_callback(std::function<void(const XmlNode&)>&& callback)
{
  this->stream_open_callbacks.emplace_back(std::move(callback));
}

void XmppParser::add_stream_close_callback(std::function<void(const XmlNode&)>&& callback)
{
  this->stream_close_callbacks.emplace_back(std::move(callback));
}
