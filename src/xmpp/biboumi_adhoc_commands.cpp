#include <xmpp/biboumi_adhoc_commands.hpp>
#include <xmpp/biboumi_component.hpp>
#include <config/config.hpp>
#include <utils/string.hpp>
#include <utils/split.hpp>
#include <xmpp/jid.hpp>
#include <algorithm>

#include <biboumi.h>

#ifdef USE_DATABASE
#include <database/database.hpp>
#endif

using namespace std::string_literals;

void DisconnectUserStep1(XmppComponent& xmpp_component, AdhocSession&, XmlNode& command_node)
{
  auto& biboumi_component = static_cast<BiboumiComponent&>(xmpp_component);

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
  for (Bridge* bridge: biboumi_component.get_bridges())
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

void DisconnectUserStep2(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  auto& biboumi_component = static_cast<BiboumiComponent&>(xmpp_component);

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
              Bridge* bridge = biboumi_component.find_user_bridge(value->get_inner());
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

void ConfigureGlobalStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  const Jid owner(session.get_owner_jid());
  const Jid target(session.get_target_jid());

  auto options = Database::get_global_options(owner.bare());

  XmlNode x("jabber:x:data:x");
  x["type"] = "form";
  XmlNode title("title");
  title.set_inner("Configure some global default settings.");
  x.add_child(std::move(title));
  XmlNode instructions("instructions");
  instructions.set_inner("Edit the form, to configure your global settings for the component.");
  x.add_child(std::move(instructions));

  XmlNode required("required");

  XmlNode max_histo_length("field");
  max_histo_length["var"] = "max_history_length";
  max_histo_length["type"] = "text-single";
  max_histo_length["label"] = "Max history length";
  max_histo_length["desc"] = "The maximum number of lines in the history that the server sends when joining a channel";

  XmlNode value("value");
  value.set_inner(std::to_string(options.maxHistoryLength.value()));
  max_histo_length.add_child(std::move(value));
  x.add_child(std::move(max_histo_length));

  XmlNode record_history("field");
  record_history["var"] = "record_history";
  record_history["type"] = "boolean";
  record_history["label"] = "Record history";
  record_history["desc"] = "Whether to save the messages into the database, or not";

  value.set_name("value");
  if (options.recordHistory.value())
    value.set_inner("true");
  else
    value.set_inner("false");
  record_history.add_child(std::move(value));
  x.add_child(std::move(record_history));

  command_node.add_child(std::move(x));
}

void ConfigureGlobalStep2(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  BiboumiComponent& biboumi_component = static_cast<BiboumiComponent&>(xmpp_component);

  const XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      const Jid owner(session.get_owner_jid());
      auto options = Database::get_global_options(owner.bare());
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        {
          const XmlNode* value = field->get_child("value", "jabber:x:data");

          if (field->get_tag("var") == "max_history_length" &&
              value && !value->get_inner().empty())
            options.maxHistoryLength = value->get_inner();
          else if (field->get_tag("var") == "record_history" &&
                   value && !value->get_inner().empty())
            {
              options.recordHistory = to_bool(value->get_inner());
              Bridge* bridge = biboumi_component.find_user_bridge(owner.bare());
              if (bridge)
                bridge->set_record_history(options.recordHistory.value());
            }
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

void ConfigureIrcServerStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  const Jid owner(session.get_owner_jid());
  const Jid target(session.get_target_jid());
  std::string server_domain;
  if ((server_domain = Config::get("fixed_irc_server", "")).empty())
    server_domain = target.local;
  auto options = Database::get_irc_server_options(owner.local + "@" + owner.domain,
                                                  server_domain);

  XmlNode x("jabber:x:data:x");
  x["type"] = "form";
  XmlNode title("title");
  title.set_inner("Configure the IRC server "s + server_domain);
  x.add_child(std::move(title));
  XmlNode instructions("instructions");
  instructions.set_inner("Edit the form, to configure the settings of the IRC server "s + server_domain);
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

  XmlNode verify_cert("field");
  verify_cert["var"] = "verify_cert";
  verify_cert["type"] = "boolean";
  verify_cert["label"] = "Verify certificate";
  verify_cert["desc"] = "Whether or not to abort the connection if the server’s TLS certificate is invalid";
  XmlNode verify_cert_value("value");
  if (options.verifyCert.value())
    verify_cert_value.set_inner("true");
  else
    verify_cert_value.set_inner("false");
  verify_cert.add_child(std::move(verify_cert_value));
  x.add_child(std::move(verify_cert));

  XmlNode fingerprint("field");
  fingerprint["var"] = "fingerprint";
  fingerprint["type"] = "text-single";
  fingerprint["label"] = "SHA-1 fingerprint of the TLS certificate to trust.";
  if (!options.trustedFingerprint.value().empty())
    {
      XmlNode fingerprint_value("value");
      fingerprint_value.set_inner(options.trustedFingerprint.value());
      fingerprint.add_child(std::move(fingerprint_value));
    }
  fingerprint.add_child(required);
  x.add_child(std::move(fingerprint));
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

  XmlNode encoding_out("field");
  encoding_out["var"] = "encoding_out";
  encoding_out["type"] = "text-single";
  encoding_out["desc"] = "The encoding used when sending messages to the IRC server.";
  encoding_out["label"] = "Out encoding";
  if (!options.encodingOut.value().empty())
    {
      XmlNode encoding_out_value("value");
      encoding_out_value.set_inner(options.encodingOut.value());
      encoding_out.add_child(std::move(encoding_out_value));
    }
  encoding_out.add_child(required);
  x.add_child(std::move(encoding_out));

  XmlNode encoding_in("field");
  encoding_in["var"] = "encoding_in";
  encoding_in["type"] = "text-single";
  encoding_in["desc"] = "The encoding used to decode message received from the IRC server.";
  encoding_in["label"] = "In encoding";
  if (!options.encodingIn.value().empty())
    {
      XmlNode encoding_in_value("value");
      encoding_in_value.set_inner(options.encodingIn.value());
      encoding_in.add_child(std::move(encoding_in_value));
    }
  encoding_in.add_child(required);
  x.add_child(std::move(encoding_in));


  command_node.add_child(std::move(x));
}

void ConfigureIrcServerStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  const XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      const Jid owner(session.get_owner_jid());
      const Jid target(session.get_target_jid());
      std::string server_domain;
      if ((server_domain = Config::get("fixed_irc_server", "")).empty())
        server_domain = target.local;
      auto options = Database::get_irc_server_options(owner.local + "@" + owner.domain,
                                                      server_domain);
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

          else if (field->get_tag("var") == "verify_cert" && value
                   && !value->get_inner().empty())
            {
              auto val = to_bool(value->get_inner());
              options.verifyCert = val;
            }

          else if (field->get_tag("var") == "fingerprint" && value &&
                   !value->get_inner().empty())
            {
              options.trustedFingerprint = value->get_inner();
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

          else if (field->get_tag("var") == "encoding_out" &&
                   value && !value->get_inner().empty())
            options.encodingOut = value->get_inner();

          else if (field->get_tag("var") == "encoding_in" &&
                   value && !value->get_inner().empty())
            options.encodingIn = value->get_inner();

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

void ConfigureIrcChannelStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  const Jid owner(session.get_owner_jid());
  const Jid target(session.get_target_jid());
  const Iid iid(target.local, {});
  auto options = Database::get_irc_channel_options_with_server_default(owner.local + "@" + owner.domain,
                                                                       iid.get_server(), iid.get_local());

  XmlNode x("jabber:x:data:x");
  x["type"] = "form";
  XmlNode title("title");
  title.set_inner("Configure the IRC channel "s + iid.get_local() + " on server "s + iid.get_server());
  x.add_child(std::move(title));
  XmlNode instructions("instructions");
  instructions.set_inner("Edit the form, to configure the settings of the IRC channel "s + iid.get_local());
  x.add_child(std::move(instructions));

  XmlNode required("required");

  XmlNode encoding_out("field");
  encoding_out["var"] = "encoding_out";
  encoding_out["type"] = "text-single";
  encoding_out["desc"] = "The encoding used when sending messages to the IRC server. Defaults to the server's “out encoding” if unset for the channel";
  encoding_out["label"] = "Out encoding";
  if (!options.encodingOut.value().empty())
    {
      XmlNode encoding_out_value("value");
      encoding_out_value.set_inner(options.encodingOut.value());
      encoding_out.add_child(std::move(encoding_out_value));
    }
  encoding_out.add_child(required);
  x.add_child(std::move(encoding_out));

  XmlNode encoding_in("field");
  encoding_in["var"] = "encoding_in";
  encoding_in["type"] = "text-single";
  encoding_in["desc"] = "The encoding used to decode message received from the IRC server. Defaults to the server's “in encoding” if unset for the channel";
  encoding_in["label"] = "In encoding";
  if (!options.encodingIn.value().empty())
    {
      XmlNode encoding_in_value("value");
      encoding_in_value.set_inner(options.encodingIn.value());
      encoding_in.add_child(std::move(encoding_in_value));
    }
  encoding_in.add_child(required);
  x.add_child(std::move(encoding_in));

  command_node.add_child(std::move(x));
}

void ConfigureIrcChannelStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  const XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      const Jid owner(session.get_owner_jid());
      const Jid target(session.get_target_jid());
      const Iid iid(target.local, {});
      auto options = Database::get_irc_channel_options(owner.local + "@" + owner.domain,
                                                       iid.get_server(), iid.get_local());
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        {
          const XmlNode* value = field->get_child("value", "jabber:x:data");

          if (field->get_tag("var") == "encoding_out" &&
              value && !value->get_inner().empty())
            options.encodingOut = value->get_inner();

          else if (field->get_tag("var") == "encoding_in" &&
                   value && !value->get_inner().empty())
            options.encodingIn = value->get_inner();
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

void DisconnectUserFromServerStep1(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  const Jid owner(session.get_owner_jid());
  if (owner.bare() != Config::get("admin", ""))
    { // A non-admin is not allowed to disconnect other users, only
      // him/herself, so we just skip this step
      auto next_step = session.get_next_step();
      next_step(xmpp_component, session, command_node);
    }
  else
    { // Send a form to select the user to disconnect
      auto& biboumi_component = static_cast<BiboumiComponent&>(xmpp_component);

      XmlNode x("jabber:x:data:x");
      x["type"] = "form";
      XmlNode title("title");
      title.set_inner("Disconnect a user from selected IRC servers");
      x.add_child(std::move(title));
      XmlNode instructions("instructions");
      instructions.set_inner("Choose a user JID");
      x.add_child(std::move(instructions));
      XmlNode jids_field("field");
      jids_field["var"] = "jid";
      jids_field["type"] = "list-single";
      jids_field["label"] = "The JID to disconnect";
      XmlNode required("required");
      jids_field.add_child(std::move(required));
      for (Bridge* bridge: biboumi_component.get_bridges())
        {
          XmlNode option("option");
          option["label"] = bridge->get_jid();
          XmlNode value("value");
          value.set_inner(bridge->get_jid());
          option.add_child(std::move(value));
          jids_field.add_child(std::move(option));
        }
      x.add_child(std::move(jids_field));
      command_node.add_child(std::move(x));
    }
}

void DisconnectUserFromServerStep2(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  // If no JID is contained in the command node, it means we skipped the
  // previous stage, and the jid to disconnect is the executor's jid
  std::string jid_to_disconnect = session.get_owner_jid();

  if (const XmlNode* x = command_node.get_child("x", "jabber:x:data"))
    {
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        if (field->get_tag("var") == "jid")
          {
            if (const XmlNode* value = field->get_child("value", "jabber:x:data"))
              jid_to_disconnect = value->get_inner();
          }
    }

  // Save that JID for the last step
  session.vars["jid"] = jid_to_disconnect;

  // Send a data form to let the user choose which server to disconnect the
  // user from
  command_node.delete_all_children();
  auto& biboumi_component = static_cast<BiboumiComponent&>(xmpp_component);

  XmlNode x("jabber:x:data:x");
  x["type"] = "form";
  XmlNode title("title");
  title.set_inner("Disconnect a user from selected IRC servers");
  x.add_child(std::move(title));
  XmlNode instructions("instructions");
  instructions.set_inner("Choose one or more servers to disconnect this JID from");
  x.add_child(std::move(instructions));
  XmlNode jids_field("field");
  jids_field["var"] = "irc-servers";
  jids_field["type"] = "list-multi";
  jids_field["label"] = "The servers to disconnect from";
  XmlNode required("required");
  jids_field.add_child(std::move(required));
  Bridge* bridge = biboumi_component.find_user_bridge(jid_to_disconnect);

  if (!bridge || bridge->get_irc_clients().empty())
    {
      XmlNode note("note");
      note["type"] = "info";
      note.set_inner("User "s + jid_to_disconnect + " is not connected to any IRC server.");
      command_node.add_child(std::move(note));
      session.terminate();
      return ;
    }

  for (const auto& pair: bridge->get_irc_clients())
    {
      XmlNode option("option");
      option["label"] = pair.first;
      XmlNode value("value");
      value.set_inner(pair.first);
      option.add_child(std::move(value));
      jids_field.add_child(std::move(option));
    }
  x.add_child(std::move(jids_field));

  XmlNode message_field("field");
  message_field["var"] = "quit-message";
  message_field["type"] = "text-single";
  message_field["label"] = "Quit message";
  XmlNode message_value("value");
  message_value.set_inner("Killed by admin");
  message_field.add_child(std::move(message_value));
  x.add_child(std::move(message_field));

  command_node.add_child(std::move(x));
}

void DisconnectUserFromServerStep3(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  const auto it = session.vars.find("jid");
  if (it == session.vars.end())
    return ;
  const auto jid_to_disconnect = it->second;

  std::vector<std::string> servers;
  std::string quit_message;

  if (const XmlNode* x = command_node.get_child("x", "jabber:x:data"))
    {
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        {
          if (field->get_tag("var") == "irc-servers")
            {
              for (const XmlNode* value: field->get_children("value", "jabber:x:data"))
                servers.push_back(value->get_inner());
            }
          else if (field->get_tag("var") == "quit-message")
            if (const XmlNode* value = field->get_child("value", "jabber:x:data"))
              quit_message = value->get_inner();
        }
    }

  auto& biboumi_component = static_cast<BiboumiComponent&>(xmpp_component);
  Bridge* bridge = biboumi_component.find_user_bridge(jid_to_disconnect);
  auto& clients = bridge->get_irc_clients();

  std::size_t number = 0;

  for (const auto& hostname: servers)
    {
      auto it = clients.find(hostname);
      if (it != clients.end())
        {
          it->second->on_error({"ERROR", {quit_message}});
          clients.erase(it);
          number++;
        }
    }
  command_node.delete_all_children();
  XmlNode note("note");
  note["type"] = "info";
  std::string msg = jid_to_disconnect + " was disconnected from " + std::to_string(number) + " IRC server";
  if (number > 1)
    msg += "s";
  msg += ".";
  note.set_inner(msg);
  command_node.add_child(std::move(note));
}
