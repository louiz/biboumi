#include <xmpp/xmpp_parser.hpp>
#include <xmpp/xmpp_stanza.hpp>

#include <iostream>

XmppParser::XmppParser():
  level(0),
  current_node(nullptr)
{
}

XmppParser::~XmppParser()
{
  if (this->current_node)
    delete this->current_node;
}

void XmppParser::startElement(const XML_Char* name, const XML_Char** attribute)
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

void XmppParser::endElement(const XML_Char* name)
{
  assert(name == this->current_node->get_name());
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

void XmppParser::charData(const XML_Char* data, int len)
{
  if (this->current_node->has_children())
    this->current_node->get_last_child()->set_tail(std::string(data, len));
  else
    this->current_node->set_inner(std::string(data, len));
}

void XmppParser::startNamespace(const XML_Char* prefix, const XML_Char* uri)
{
  std::cout << "startNamespace: " << prefix << ":" << uri << std::endl;
  this->namespaces.emplace(std::make_pair(prefix, uri));
}

void XmppParser::stanza_event(const Stanza& stanza) const
{
  for (const auto& callback: this->stanza_callbacks)
    callback(stanza);
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

void XmppParser::endNamespace(const XML_Char* coucou)
{
  std::cout << "endNamespace: " << coucou << std::endl;
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
