#include <utility>
#include <xmpp/adhoc_command.hpp>
#include <xmpp/xmpp_component.hpp>
#include <utils/reload.hpp>

using namespace std::string_literals;

AdhocCommand::AdhocCommand(std::vector<AdhocStep>&& callbacks, std::string name, const bool admin_only):
  name(std::move(name)),
  callbacks(std::move(callbacks)),
  admin_only(admin_only)
{
}

bool AdhocCommand::is_admin_only() const
{
  return this->admin_only;
}

void PingStep1(XmppComponent&, AdhocSession&, XmlNode& command_node)
{
  XmlSubNode note(command_node, "note");
  note["type"] = "info";
  note.set_inner("Pong");
}

void HelloStep1(XmppComponent&, AdhocSession&, XmlNode& command_node)
{
  XmlSubNode x(command_node, "jabber:x:data:x");
  x["type"] = "form";
  XmlSubNode title(x, "title");
  title.set_inner("Configure your name.");
  XmlSubNode instructions(x, "instructions");
  instructions.set_inner("Please provide your name.");
  XmlSubNode name_field(x, "field");
  name_field["var"] = "name";
  name_field["type"] = "text-single";
  name_field["label"] = "Your name";
  XmlSubNode required(name_field, "required");
}

void HelloStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  // Find out if the name was provided in the form.
  if (const XmlNode* x = command_node.get_child("x", "jabber:x:data"))
    {
      const XmlNode* name_field = nullptr;
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        if (field->get_tag("var") == "name")
          {
            name_field = field;
            break;
          }
      if (name_field)
        {
          if (const XmlNode* value = name_field->get_child("value", "jabber:x:data"))
            {
              const std::string value_str = value->get_inner();
              command_node.delete_all_children();
              XmlSubNode note(command_node, "note");
              note["type"] = "info";
              note.set_inner("Hello "s + value_str + "!"s);
              return;
            }
        }
    }
  command_node.delete_all_children();
  XmlSubNode error(command_node, ADHOC_NS":error");
  error["type"] = "modify";
  XmlSubNode condition(error, STANZA_NS":bad-request");
  session.terminate();
}

void Reload(XmppComponent&, AdhocSession&, XmlNode& command_node)
{
  ::reload_process();
  command_node.delete_all_children();
  XmlSubNode note(command_node, "note");
  note["type"] = "info";
  note.set_inner("Configuration reloaded.");
}
