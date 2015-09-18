#include <xmpp/biboumi_adhoc_commands.hpp>
#include <xmpp/biboumi_component.hpp>
#include <bridge/bridge.hpp>
#include <utils/string.hpp>
#include <xmpp/jid.hpp>

#include <biboumi.h>

#ifdef USE_DATABASE
#include <database/database.hpp>
#endif

using namespace std::string_literals;

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

#ifdef USE_DATABASE
void ConfigureIrcServerStep1(XmppComponent* xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  auto biboumi_component = static_cast<BiboumiComponent*>(xmpp_component);

  const Jid owner(session.get_owner_jid());
  const Jid target(session.get_target_jid());
  auto options = Database::get_irc_server_options(owner.local + "@" + owner.domain,
                                                  target.local);

  XmlNode x("jabber:x:data:x");
  x["type"] = "form";
  XmlNode title("title");
  title.set_inner("Configure the IRC server "s + target.local);
  x.add_child(std::move(title));
  XmlNode instructions("instructions");
  instructions.set_inner("Edit the form, to configure the settings of the IRC server "s + target.local);
  x.add_child(std::move(instructions));

  XmlNode require_tls("field");
  require_tls["var"] = "require_tls";
  require_tls["type"] = "boolean";
  require_tls["label"] = "Require TLS (refuse to connect insecurely)";
  XmlNode require_tls_value("value");
  require_tls_value.set_inner(options.requireTls ? "true": "false");
  require_tls.add_child(std::move(require_tls_value));
  XmlNode required("required");
  require_tls.add_child(required);
  x.add_child(std::move(require_tls));

  XmlNode pass("field");
  pass["var"] = "pass";
  pass["type"] = "text-private";
  pass["label"] = "Server password (to be used in a PASS command when connecting)";
  if (!options.pass.value().empty())
    {
      XmlNode pass_value("value");
      pass_value.set_inner(options.pass.value());
      pass.add_child(std::move(pass_value));
    }
  pass.add_child(required);
  x.add_child(std::move(pass));

  command_node.add_child(std::move(x));
}

void ConfigureIrcServerStep2(XmppComponent* xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  auto biboumi_component = static_cast<BiboumiComponent*>(xmpp_component);

  const XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      const Jid owner(session.get_owner_jid());
      const Jid target(session.get_target_jid());
      auto options = Database::get_irc_server_options(owner.local + "@" + owner.domain,
                                                      target.local);
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        {
          const XmlNode* value = field->get_child("value", "jabber:x:data");
          if (field->get_tag("var") == "require_tls" &&
              value && !value->get_inner().empty())
            options.requireTls = to_bool(value->get_inner());

          else if (field->get_tag("var") == "pass" &&
              value && !value->get_inner().empty())
            options.pass = value->get_inner();
        }

      options.update();

      command_node.delete_all_children();
      XmlNode note("note");
      note["type"] = "info";
      note.set_inner("Configuration successfully applied.");
      command_node.add_child(std::move(note));
      return;
    }
  XmlNode error(ADHOC_NS":error");
  error["type"] = "modify";
  XmlNode condition(STANZA_NS":bad-request");
  error.add_child(std::move(condition));
  command_node.add_child(std::move(error));
  session.terminate();
}
#endif  // USE_DATABASE
