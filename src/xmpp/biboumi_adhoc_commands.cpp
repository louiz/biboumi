#include <xmpp/biboumi_adhoc_commands.hpp>
#include <xmpp/biboumi_component.hpp>
#include <bridge/bridge.hpp>

void DisconnectUserStep1(XmppComponent* xmpp_component, AdhocSession&, XmlNode& command_node)
{
  auto biboumi_component = static_cast<BiboumiComponent*>(xmpp_component);

  XmlNode x("jabber:x:data:x");
  x["type"] = "form";
  XmlNode title("title");
  title.set_inner("Disconnect a user from the gateway");
  x.add_child(std::move(title));
  XmlNode instructions("instructions");
  instructions.set_inner("Choose a user JID and a quit message");
  x.add_child(std::move(instructions));
  XmlNode jids_field("field");
  jids_field["var"] = "jids";
  jids_field["type"] = "list-multi";
  jids_field["label"] = "The JIDs to disconnect";
  XmlNode required("required");
  jids_field.add_child(std::move(required));
  for (Bridge* bridge: biboumi_component->get_bridges())
    {
      XmlNode option("option");
      option["label"] = bridge->get_jid();
      XmlNode value("value");
      value.set_inner(bridge->get_jid());
      option.add_child(std::move(value));
      jids_field.add_child(std::move(option));
    }
  x.add_child(std::move(jids_field));

  XmlNode message_field("field");
  message_field["var"] = "quit-message";
  message_field["type"] = "text-single";
  message_field["label"] = "Quit message";
  XmlNode message_value("value");
  message_value.set_inner("Disconnected by admin");
  message_field.add_child(std::move(message_value));
  x.add_child(std::move(message_field));
  command_node.add_child(std::move(x));
}

void DisconnectUserStep2(XmppComponent* xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  auto biboumi_component = static_cast<BiboumiComponent*>(xmpp_component);

  // Find out if the jids, and the quit message are provided in the form.
  std::string quit_message;
  const XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      const XmlNode* message_field = nullptr;
      const XmlNode* jids_field = nullptr;
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        if (field->get_tag("var") == "jids")
          jids_field = field;
        else if (field->get_tag("var") == "quit-message")
          message_field = field;
      if (message_field)
        {
          const XmlNode* value = message_field->get_child("value", "jabber:x:data");
          if (value)
            quit_message = value->get_inner();
        }
      if (jids_field)
        {
          std::size_t num = 0;
          for (const XmlNode* value: jids_field->get_children("value", "jabber:x:data"))
            {
              Bridge* bridge = biboumi_component->find_user_bridge(value->get_inner());
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
          command_node.add_child(std::move(note));
          return;
        }
    }
  XmlNode error(ADHOC_NS":error");
  error["type"] = "modify";
  XmlNode condition(STANZA_NS":bad-request");
  error.add_child(std::move(condition));
  command_node.add_child(std::move(error));
  session.terminate();
}
