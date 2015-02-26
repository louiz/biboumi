#include <xmpp/adhoc_command.hpp>
#include <xmpp/xmpp_component.hpp>

#include <bridge/bridge.hpp>

#include <utils/reload.hpp>

using namespace std::string_literals;

AdhocCommand::AdhocCommand(std::vector<AdhocStep>&& callbacks, const std::string& name, const bool admin_only):
  name(name),
  callbacks(std::move(callbacks)),
  admin_only(admin_only)
{
}

AdhocCommand::~AdhocCommand()
{
}

bool AdhocCommand::is_admin_only() const
{
  return this->admin_only;
}

void PingStep1(XmppComponent*, AdhocSession&, XmlNode& command_node)
{
  XmlNode note("note");
  note["type"] = "info";
  note.set_inner("Pong");
  note.close();
  command_node.add_child(std::move(note));
}

void HelloStep1(XmppComponent*, AdhocSession&, XmlNode& command_node)
{
  XmlNode x("jabber:x:data:x");
  x["type"] = "form";
  XmlNode title("title");
  title.set_inner("Configure your name.");
  title.close();
  x.add_child(std::move(title));
  XmlNode instructions("instructions");
  instructions.set_inner("Please provide your name.");
  instructions.close();
  x.add_child(std::move(instructions));
  XmlNode name_field("field");
  name_field["var"] = "name";
  name_field["type"] = "text-single";
  name_field["label"] = "Your name";
  XmlNode required("required");
  required.close();
  name_field.add_child(std::move(required));
  name_field.close();
  x.add_child(std::move(name_field));
  x.close();
  command_node.add_child(std::move(x));
}

void HelloStep2(XmppComponent*, AdhocSession& session, XmlNode& command_node)
{
  // Find out if the name was provided in the form.
  XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      XmlNode* name_field = nullptr;
      for (XmlNode* field: x->get_children("field", "jabber:x:data"))
        if (field->get_tag("var") == "name")
          {
            name_field = field;
            break;
          }
      if (name_field)
        {
          XmlNode* value = name_field->get_child("value", "jabber:x:data");
          if (value)
            {
              XmlNode note("note");
              note["type"] = "info";
              note.set_inner("Hello "s + value->get_inner() + "!"s);
              note.close();
              command_node.delete_all_children();
              command_node.add_child(std::move(note));
              return;
            }
        }
    }
  command_node.delete_all_children();
  XmlNode error(ADHOC_NS":error");
  error["type"] = "modify";
  XmlNode condition(STANZA_NS":bad-request");
  condition.close();
  error.add_child(std::move(condition));
  error.close();
  command_node.add_child(std::move(error));
  session.terminate();
}

void DisconnectUserStep1(XmppComponent* xmpp_component, AdhocSession&, XmlNode& command_node)
{
  XmlNode x("jabber:x:data:x");
  x["type"] = "form";
  XmlNode title("title");
  title.set_inner("Disconnect a user from the gateway");
  title.close();
  x.add_child(std::move(title));
  XmlNode instructions("instructions");
  instructions.set_inner("Choose a user JID and a quit message");
  instructions.close();
  x.add_child(std::move(instructions));
  XmlNode jids_field("field");
  jids_field["var"] = "jids";
  jids_field["type"] = "list-multi";
  jids_field["label"] = "The JIDs to disconnect";
  XmlNode required("required");
  required.close();
  jids_field.add_child(std::move(required));
  for (Bridge* bridge: xmpp_component->get_bridges())
    {
      XmlNode option("option");
      option["label"] = bridge->get_jid();
      XmlNode value("value");
      value.set_inner(bridge->get_jid());
      value.close();
      option.add_child(std::move(value));
      option.close();
      jids_field.add_child(std::move(option));
    }
  jids_field.close();
  x.add_child(std::move(jids_field));

  XmlNode message_field("field");
  message_field["var"] = "quit-message";
  message_field["type"] = "text-single";
  message_field["label"] = "Quit message";
  XmlNode message_value("value");
  message_value.set_inner("Disconnected by admin");
  message_value.close();
  message_field.add_child(std::move(message_value));
  message_field.close();
  x.add_child(std::move(message_field));
  x.close();
  command_node.add_child(std::move(x));
}

void DisconnectUserStep2(XmppComponent* xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  // Find out if the jids, and the quit message are provided in the form.
  std::string quit_message;
  XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      XmlNode* message_field = nullptr;
      XmlNode* jids_field = nullptr;
      for (XmlNode* field: x->get_children("field", "jabber:x:data"))
        if (field->get_tag("var") == "jids")
          jids_field = field;
        else if (field->get_tag("var") == "quit-message")
          message_field = field;
      if (message_field)
        {
          XmlNode* value = message_field->get_child("value", "jabber:x:data");
          if (value)
            quit_message = value->get_inner();
        }
      if (jids_field)
        {
          std::size_t num = 0;
          for (XmlNode* value: jids_field->get_children("value", "jabber:x:data"))
            {
              Bridge* bridge = xmpp_component->find_user_bridge(value->get_inner());
              if (bridge)
                {
                  bridge->shutdown(quit_message);
                  num++;
                }
            }
          command_node.delete_all_children();

          XmlNode note("note");
          note["type"] = "info";
          if (num == 0)
            note.set_inner("No user were disconnected.");
          else if (num == 1)
            note.set_inner("1 user has been disconnected.");
          else
            note.set_inner(std::to_string(num) + " users have been disconnected.");
          note.close();
          command_node.add_child(std::move(note));
          return;
        }
    }
  XmlNode error(ADHOC_NS":error");
  error["type"] = "modify";
  XmlNode condition(STANZA_NS":bad-request");
  condition.close();
  error.add_child(std::move(condition));
  error.close();
  command_node.add_child(std::move(error));
  session.terminate();
}

void Reload(XmppComponent*, AdhocSession&, XmlNode& command_node)
{
  ::reload_process();
  command_node.delete_all_children();
  XmlNode note("note");
  note["type"] = "info";
  note.set_inner("Configuration reloaded.");
  note.close();
  command_node.add_child(std::move(note));
}
