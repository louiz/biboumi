#ifndef XMPP_PARSER_INCLUDED
# define XMPP_PARSER_INCLUDED

#include <functional>
#include <stack>

#include <xmpp/xmpp_stanza.hpp>

#include <expatpp.h>

/**
 * A SAX XML parser that builds XML nodes and spawns events when a complete
 * stanza is received (an element of level 2), or when the document is
 * opened (an element of level 1)
 *
 * After a stanza_event has been spawned, we delete the whole stanza. This
 * means that even with a very long document (in XMPP the document is
 * potentially infinite), the memory then is never exhausted as long as each
 * stanza is reasonnably short.
 *
 * TODO: enforce the size-limit for the stanza (limit the number of childs
 * it can contain). For example forbid the parser going further than level
 * 20 (arbitrary number here), and each XML node to have more than 15 childs
 * (arbitrary number again).
 */
class XmppParser: public expatpp
{
public:
  explicit XmppParser();
  ~XmppParser();

public:
  /**
   * Add one callback for the various events that this parser can spawn.
   */
  void add_stanza_callback(std::function<void(const Stanza&)>&& callback);
  void add_stream_open_callback(std::function<void(const XmlNode&)>&& callback);
  void add_stream_close_callback(std::function<void(const XmlNode&)>&& callback);

private:
  /**
   * Called when a new XML element has been opened. We instanciate a new
   * XmlNode and set it as our current node. The parent of this new node is
   * the previous "current" node. We have all the element's attributes in
   * this event.
   *
   * We spawn a stream_event with this node if this is a level-1 element.
   */
  void startElement(const XML_Char* name, const XML_Char** attribute);
  /**
   * Called when an XML element has been closed. We close the current_node,
   * set our current_node as the parent of the current_node, and if that was
   * a level-2 element we spawn a stanza_event with this node.
   *
   * And we then delete the stanza (and everything under it, its children,
   * attribute, etc).
   */
  void endElement(const XML_Char* name);
  /**
   * Some inner or tail data has been parsed
   */
  void charData(const XML_Char* data, int len);
  /**
   * TODO use that.
   */
  void startNamespace(const XML_Char* prefix, const XML_Char* uri);
  /**
   * TODO and that.
   */
  void endNamespace(const XML_Char* prefix);
  /**
   * Calls all the stanza_callbacks one by one.
   */
  void stanza_event(const Stanza& stanza) const;
  /**
   * Calls all the stream_open_callbacks one by one. Note: the passed node is not
   * closed yet.
   */
  void stream_open_event(const XmlNode& node) const;
  /**
   * Calls all the stream_close_callbacks one by one.
   */
  void stream_close_event(const XmlNode& node) const;

  /**
   * The current depth in the XML document
   */
  size_t level;
  /**
   * The deepest XML node opened but not yet closed (to which we are adding
   * new children, inner or tail)
   */
  XmlNode* current_node;
  /**
   * A list of callbacks to be called on an *_event, receiving the
   * concerned Stanza/XmlNode.
   */
  std::vector<std::function<void(const Stanza&)>> stanza_callbacks;
  std::vector<std::function<void(const XmlNode&)>> stream_open_callbacks;
  std::vector<std::function<void(const XmlNode&)>> stream_close_callbacks;
  /**
   * TODO: also use that.
   */
  std::stack<std::pair<std::string, std::string>> namespaces;
};

#endif // XMPP_PARSER_INCLUDED
