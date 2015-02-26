#include <xmpp/xmpp_stanza.hpp>

#include <utils/encoding.hpp>
#include <utils/split.hpp>

#include <stdexcept>
#include <iostream>

#include <string.h>

std::string xml_escape(const std::string& data)
{
  std::string res;
  res.reserve(data.size());
  for (size_t pos = 0; pos != data.size(); ++pos)
    {
      switch(data[pos])
        {
        case '&':
          res += "&amp;";
          break;
        case '<':
          res += "&lt;";
          break;
        case '>':
          res += "&gt;";
          break;
        case '\"':
          res += "&quot;";
          break;
        case '\'':
          res += "&apos;";
          break;
        default:
          res += data[pos];
          break;
        }
    }
  return res;
}

std::string xml_unescape(const std::string& data)
{
  std::string res;
  res.reserve(data.size());
  const char* str = data.c_str();
  while (str && *str && static_cast<size_t>(str - data.c_str()) < data.size())
    {
      if (*str == '&')
        {
          if (strncmp(str+1, "amp;", 4) == 0)
            {
              res += "&";
              str += 4;
            }
          else if (strncmp(str+1, "lt;", 3) == 0)
            {
              res += "<";
              str += 3;
            }
          else if (strncmp(str+1, "gt;", 3) == 0)
            {
              res += ">";
              str += 3;
            }
          else if (strncmp(str+1, "quot;", 5) == 0)
            {
              res += "\"";
              str += 5;
            }
          else if (strncmp(str+1, "apos;", 5) == 0)
            {
              res += "'";
              str += 5;
            }
          else
            res += "&";
        }
      else
        res += *str;
      str++;
    }
  return res;
}

XmlNode::XmlNode(const std::string& name, XmlNode* parent):
  parent(parent),
  closed(false)
{
  // split the namespace and the name
  auto n = name.rfind(":");
  if (n == std::string::npos)
    this->name = name;
  else
    {
      this->name = name.substr(n+1);
      this->attributes["xmlns"] = name.substr(0, n);
    }
}

XmlNode::XmlNode(const std::string& name):
  XmlNode(name, nullptr)
{
}

XmlNode::~XmlNode()
{
  this->delete_all_children();
}

void XmlNode::delete_all_children()
{
  for (auto& child: this->children)
    {
      delete child;
    }
  this->children.clear();
}

void XmlNode::set_attribute(const std::string& name, const std::string& value)
{
  this->attributes[name] = value;
}

void XmlNode::set_tail(const std::string& data)
{
  this->tail = xml_escape(data);
}

void XmlNode::add_to_tail(const std::string& data)
{
  this->tail += xml_escape(data);
}

void XmlNode::set_inner(const std::string& data)
{
  this->inner = xml_escape(data);
}

void XmlNode::add_to_inner(const std::string& data)
{
  this->inner += xml_escape(data);
}

std::string XmlNode::get_inner() const
{
  return xml_unescape(this->inner);
}

std::string XmlNode::get_tail() const
{
  return xml_unescape(this->tail);
}

XmlNode* XmlNode::get_child(const std::string& name, const std::string& xmlns) const
{
  for (auto& child: this->children)
    {
      if (child->name == name && child->get_tag("xmlns") == xmlns)
        return child;
    }
  return nullptr;
}

std::vector<XmlNode*> XmlNode::get_children(const std::string& name, const std::string& xmlns) const
{
  std::vector<XmlNode*> res;
  for (auto& child: this->children)
    {
      if (child->name == name && child->get_tag("xmlns") == xmlns)
        res.push_back(child);
    }
  return res;
}

XmlNode* XmlNode::add_child(XmlNode* child)
{
  child->parent = this;
  this->children.push_back(child);
  return child;
}

XmlNode* XmlNode::add_child(XmlNode&& child)
{
  XmlNode* new_node = new XmlNode(std::move(child));
  return this->add_child(new_node);
}

XmlNode* XmlNode::get_last_child() const
{
  return this->children.back();
}

void XmlNode::close()
{
  if (this->closed)
    throw std::runtime_error("Closing an already closed XmlNode");
  this->closed = true;
}

XmlNode* XmlNode::get_parent() const
{
  return this->parent;
}

void XmlNode::set_name(const std::string& name)
{
  this->name = name;
}

const std::string XmlNode::get_name() const
{
  return this->name;
}

std::string XmlNode::to_string() const
{
  std::string res("<");
  res += this->name;
  for (const auto& it: this->attributes)
    res += " " + it.first + "='" + sanitize(it.second) + "'";
  if (this->closed && !this->has_children() && this->inner.empty())
    res += "/>";
  else
    {
      res += ">" + sanitize(this->inner);
      for (const auto& child: this->children)
        res += child->to_string();
      if (this->closed)
        {
          res += "</" + this->get_name() + ">";
        }
    }
  res += sanitize(this->tail);
  return res;
}

bool XmlNode::has_children() const
{
  return !this->children.empty();
}

const std::string XmlNode::get_tag(const std::string& name) const
{
  try
    {
      const auto& value = this->attributes.at(name);
      return value;
    }
  catch (const std::out_of_range& e)
    {
      return "";
    }
}

bool XmlNode::del_tag(const std::string& name)
{
  if (this->attributes.erase(name) != 0)
    return true;
  return false;
}

std::string& XmlNode::operator[](const std::string& name)
{
  return this->attributes[name];
}

std::string sanitize(const std::string& data)
{
  if (utils::is_valid_utf8(data.data()))
    return xml_escape(utils::remove_invalid_xml_chars(data));
  else
    return xml_escape(utils::remove_invalid_xml_chars(utils::convert_to_utf8(data, "ISO-8859-1")));
}
