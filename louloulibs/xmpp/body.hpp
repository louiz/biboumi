#pragma once


namespace Xmpp
{
// Contains:
// - an XMPP-valid UTF-8 body
// - an XML node representing the XHTML-IM body, or null
  using body = std::tuple<const std::string, std::unique_ptr<XmlNode>>;
}


