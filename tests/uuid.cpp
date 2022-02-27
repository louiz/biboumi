#include "catch2/catch.hpp"

#include <xmpp/xmpp_component.hpp>

TEST_CASE("id generation")
{
  const std::string first_uuid = XmppComponent::next_id();
  const std::string second_uuid = XmppComponent::next_id();

  CHECK(first_uuid.size() == 36);
  CHECK(second_uuid.size() == 36);
  CHECK(first_uuid != second_uuid);
}
