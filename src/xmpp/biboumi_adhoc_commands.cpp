#include <xmpp/biboumi_adhoc_commands.hpp>
#include <xmpp/biboumi_component.hpp>
#include <config/config.hpp>
#include <utils/string.hpp>
#include <utils/split.hpp>
#include <xmpp/jid.hpp>

#include <biboumi.h>

#ifdef USE_DATABASE
#include <database/database.hpp>
#endif

#include <louloulibs.h>

#include <algorithm>

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
void ConfigureIrcServerStep1(XmppComponent*, AdhocSession& session, XmlNode& command_node)
{
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

  XmlNode required("required");

  XmlNode ports("field");
  ports["var"] = "ports";
  ports["type"] = "text-multi";
  ports["label"] = "Ports";
  ports["desc"] = "List of ports to try, without TLS. Defaults: 6667.";
  auto vals = utils::split(options.ports.value(), ';', false);
  for (const auto& val: vals)
    {
      XmlNode ports_value("value");
      ports_value.set_inner(val);
      ports.add_child(std::move(ports_value));
    }
  ports.add_child(required);
  x.add_child(std::move(ports));

#ifdef BOTAN_FOUND
  XmlNode tls_ports("field");
  tls_ports["var"] = "tls_ports";
  tls_ports["type"] = "text-multi";
  tls_ports["label"] = "TLS ports";
  tls_ports["desc"] = "List of ports to try, with TLS. Defaults: 6697, 6670.";
  vals = utils::split(options.tlsPorts.value(), ';', false);
  for (const auto& val: vals)
    {
      XmlNode tls_ports_value("value");
      tls_ports_value.set_inner(val);
      tls_ports.add_child(std::move(tls_ports_value));
    }
  tls_ports.add_child(required);
  x.add_child(std::move(tls_ports));
#endif

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

  XmlNode after_cnt_cmd("field");
  after_cnt_cmd["var"] = "after_connect_command";
  after_cnt_cmd["type"] = "text-single";
  after_cnt_cmd["desc"] = "Custom IRC command sent after the connection is established with the server.";
  after_cnt_cmd["label"] = "After-connection IRC command";
  if (!options.afterConnectionCommand.value().empty())
    {
      XmlNode after_cnt_cmd_value("value");
      after_cnt_cmd_value.set_inner(options.afterConnectionCommand.value());
      after_cnt_cmd.add_child(std::move(after_cnt_cmd_value));
    }
  after_cnt_cmd.add_child(required);
  x.add_child(std::move(after_cnt_cmd));

  if (Config::get("realname_customization", "true") == "true")
    {
      XmlNode username("field");
      username["var"] = "username";
      username["type"] = "text-single";
      username["label"] = "Username";
      if (!options.username.value().empty())
        {
          XmlNode username_value("value");
          username_value.set_inner(options.username.value());
          username.add_child(std::move(username_value));
        }
      username.add_child(required);
      x.add_child(std::move(username));

      XmlNode realname("field");
      realname["var"] = "realname";
      realname["type"] = "text-single";
      realname["label"] = "Realname";
      if (!options.realname.value().empty())
        {
          XmlNode realname_value("value");
          realname_value.set_inner(options.realname.value());
          realname.add_child(std::move(realname_value));
        }
      realname.add_child(required);
      x.add_child(std::move(realname));
    }

  command_node.add_child(std::move(x));
}

void ConfigureIrcServerStep2(XmppComponent*, AdhocSession& session, XmlNode& command_node)
{
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
          const std::vector<const XmlNode*> values = field->get_children("value", "jabber:x:data");
          if (field->get_tag("var") == "ports")
            {
              std::string ports;
              for (const auto& val: values)
                ports += val->get_inner() + ";";
              options.ports = ports;
            }

#ifdef BOTAN_FOUND
          else if (field->get_tag("var") == "tls_ports")
            {
              std::string ports;
              for (const auto& val: values)
                ports += val->get_inner() + ";";
              options.tlsPorts = ports;
            }
#endif // BOTAN_FOUND

          else if (field->get_tag("var") == "pass" &&
                   value && !value->get_inner().empty())
            options.pass = value->get_inner();

          else if (field->get_tag("var") == "after_connect_command" &&
                   value && !value->get_inner().empty())
            options.afterConnectionCommand = value->get_inner();

          else if (field->get_tag("var") == "username" &&
                   value && !value->get_inner().empty())
            {
              auto username = value->get_inner();
              // The username must not contain spaces
              std::replace(username.begin(), username.end(), ' ', '_');
              options.username = username;
            }

          else if (field->get_tag("var") == "realname" &&
                   value && !value->get_inner().empty())
            options.realname = value->get_inner();
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
