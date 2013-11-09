#include <xmpp/xmpp_stanza.hpp>

#include <utils/encoding.hpp>

#include <iostream>

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


XmlNode::XmlNode(const std::string& name, XmlNode* parent):
  name(name),
  parent(parent),
  closed(false)
{
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
  this->tail = data;
}

void XmlNode::set_inner(const std::string& data)
{
  this->inner = xml_escape(data);
}

std::string XmlNode::get_inner() const
{
  return this->inner;
}

XmlNode* XmlNode::get_child(const std::string& name) const
{
  for (auto& child: this->children)
    {
      if (child->name == name)
        return child;
    }
  return nullptr;
}

void XmlNode::add_child(XmlNode* child)
{
  this->children.push_back(child);
}

void XmlNode::add_child(XmlNode&& child)
{
  XmlNode* new_node = new XmlNode(std::move(child));
  this->add_child(new_node);
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

const std::string& XmlNode::get_name() const
{
  return this->name;
}

std::string XmlNode::to_string() const
{
  std::string res("<");
  res += this->name;
  for (const auto& it: this->attributes)
    res += " " + it.first + "='" + it.second + "'";
  if (this->closed && !this->has_children() && this->inner.empty())
    res += "/>";
  else
    {
      res += ">" + this->inner;
      for (const auto& child: this->children)
        res += child->to_string();
      if (this->closed)
        {
          res += "</" + this->name + ">";
        }
    }
  res += this->tail;
  return res;
}

void XmlNode::display() const
{
  std::cout << this->to_string() << std::endl;
}

bool XmlNode::has_children() const
{
  return !this->children.empty();
}

const std::string& XmlNode::operator[](const std::string& name) const
{
  try
    {
      const auto& value = this->attributes.at(name);
      return value;
    }
  catch (const std::out_of_range& e)
    {
      throw AttributeNotFound();
    }
}

std::string& XmlNode::operator[](const std::string& name)
{
  return this->attributes[name];
}
