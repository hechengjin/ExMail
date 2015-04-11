/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Test suite for basic nsIAbCard functions.
 */

// Intersperse these with UTF-8 values to check we handle them correctly.
const kFNValue = "testFirst\u00D0";
const kLNValue = "testLast";
const kDNValue = "testDisplay\u00D1";
const kEmailValue = "testEmail\u00D2@invalid.com";
const kEmailValueLC = "testemail\u00D2@invalid.com";
const kEmailValue2 = "test@test.invalid.com";
// Email without the @ or anything after it.
const kEmailReducedValue = "testEmail\u00D2";
const kCompanyValue = "Test\u00D0 Company";

function run_test() {
  // Create a new card
  var card = Components.classes["@mozilla.org/addressbook/cardproperty;1"]
                       .createInstance(Components.interfaces.nsIAbCard);

  // Test - Set First, Last and Display Names and Email Address
  // via setProperty, and check correctly saved via their
  // attributes. We're using firstName to check UTF-8 values.
  card.setProperty("FirstName", kFNValue);
  card.setProperty("LastName", kLNValue);
  card.setProperty("DisplayName", kDNValue);
  card.setProperty("PrimaryEmail", kEmailValue);

  do_check_eq(card.firstName, kFNValue);
  do_check_eq(card.lastName, kLNValue);
  do_check_eq(card.displayName, kDNValue);
  do_check_eq(card.primaryEmail, kEmailValue);

  // Repeat in the opposite order.
  card.firstName = kFNValue;
  card.lastName = kLNValue;
  card.displayName = kDNValue;
  card.primaryEmail = kEmailValue;

  do_check_eq(card.getProperty("FirstName", "BAD"), kFNValue);
  do_check_eq(card.getProperty("LastName", "BAD"), kLNValue);
  do_check_eq(card.getProperty("DisplayName", "BAD"), kDNValue);
  do_check_eq(card.getProperty("PrimaryEmail", "BAD"), kEmailValue);

  // Test - generateName. Note: if the addressBook.properties
  // value changes, this will affect these tests.

  // Add a company name, so we can test fallback to company name.
  card.setProperty("Company", kCompanyValue);

  do_check_eq(card.generateName(0), kDNValue);
  do_check_eq(card.generateName(1), kLNValue + ", " + kFNValue);
  do_check_eq(card.generateName(2), kFNValue + " " + kLNValue);

  // Test - generateName, with missing items.

  card.displayName = "";
  do_check_eq(card.generateName(0), kCompanyValue);

  card.deleteProperty("Company");
  do_check_eq(card.generateName(0), kEmailReducedValue);

  // Reset company name for the first/last name tests.
  card.setProperty("Company", kCompanyValue);

  card.firstName = "";
  do_check_eq(card.generateName(1), kLNValue);
  do_check_eq(card.generateName(2), kLNValue);

  card.firstName = kFNValue;
  card.lastName = "";
  do_check_eq(card.generateName(1), kFNValue);
  do_check_eq(card.generateName(2), kFNValue);

  card.firstName = "";
  do_check_eq(card.generateName(1), kCompanyValue);
  do_check_eq(card.generateName(2), kCompanyValue);

  card.deleteProperty("Company");
  do_check_eq(card.generateName(1), kEmailReducedValue);
  do_check_eq(card.generateName(2), kEmailReducedValue);

  card.primaryEmail = "";
  do_check_eq(card.generateName(1), "");
  do_check_eq(card.generateName(2), "");

  // Test - generateNameWithBundle, most of this will have
  // been tested above.

  card.firstName = kFNValue;
  card.lastName = kLNValue;

  let bundle = Services.strings.createBundle("chrome://messenger/locale/addressbook/addressBook.properties");

  do_check_eq(card.generateName(1, bundle), kLNValue + ", " + kFNValue);

  // Test - generatePhoneticName

  card.setProperty("PhoneticFirstName", kFNValue);
  card.setProperty("PhoneticLastName", kLNValue);
  do_check_eq(card.generatePhoneticName(false), kFNValue + kLNValue);
  do_check_eq(card.generatePhoneticName(true), kLNValue + kFNValue);

  card.setProperty("PhoneticLastName", "");
  do_check_eq(card.generatePhoneticName(false), kFNValue);
  do_check_eq(card.generatePhoneticName(true), kFNValue);

  card.setProperty("PhoneticFirstName", "");
  card.setProperty("PhoneticLastName", kLNValue);
  do_check_eq(card.generatePhoneticName(false), kLNValue);
  do_check_eq(card.generatePhoneticName(true), kLNValue);

  // Test - hasEmailAddress

  card.deleteProperty("PrimaryEmail");
  card.deleteProperty("SecondEmail");

  do_check_eq(card.hasEmailAddress(kEmailValue), false);
  do_check_eq(card.hasEmailAddress(kEmailValueLC), false);
  do_check_eq(card.hasEmailAddress(kEmailValue2), false);

  card.setProperty("PrimaryEmail", kEmailValue);

  do_check_eq(card.hasEmailAddress(kEmailValue), true);
  do_check_eq(card.hasEmailAddress(kEmailValueLC), true);
  do_check_eq(card.hasEmailAddress(kEmailValue2), false);

  card.setProperty("SecondEmail", kEmailValue2);

  do_check_eq(card.hasEmailAddress(kEmailValue), true);
  do_check_eq(card.hasEmailAddress(kEmailValueLC), true);
  do_check_eq(card.hasEmailAddress(kEmailValue2), true);

  card.deleteProperty("PrimaryEmail");

  do_check_eq(card.hasEmailAddress(kEmailValue), false);
  do_check_eq(card.hasEmailAddress(kEmailValueLC), false);
  do_check_eq(card.hasEmailAddress(kEmailValue2), true);
}
