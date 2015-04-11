/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
    test_folding();
    test_icalProps();
    test_roundtrip();
}

function test_roundtrip() {
    let ics_xmas =
        "BEGIN:VCALENDAR\n" +
        "PRODID:-//ORACLE//NONSGML CSDK 9.0.5 - CalDAVServlet 9.0.5//EN\n" +
        "VERSION:2.0\n" +
        "BEGIN:VEVENT\n" +
        "UID:20041119T052239Z-1000472-1-5c0746bb-Oracle\n" +
        "ORGANIZER;X-ORACLE-GUID=E9359406791C763EE0305794071A39A4;CN=Simon Vaillan\n" +
        "court:mailto:simon.vaillancourt@oracle.com\n" +
        "SEQUENCE:0\n" +
        "DTSTAMP:20041124T010028Z\n" +
        "CREATED:20041119T052239Z\n" +
        "X-ORACLE-EVENTINSTANCE-GUID:I1+16778354+1+1+438153759\n" +
        "X-ORACLE-EVENT-GUID:E1+16778354+1+438153759\n" +
        "X-ORACLE-EVENTTYPE:DAY EVENT\n" +
        "TRANSP:TRANSPARENT\n" +
        "SUMMARY:Christmas\n" +
        "STATUS:CONFIRMED\n" +
        "PRIORITY:0\n" +
        "DTSTART;VALUE=DATE:20041125\n" +
        "DTEND;VALUE=DATE:20041125\n" +
        "CLASS:PUBLIC\n" +
        "ATTENDEE;X-ORACLE-GUID=E92F51FB4A48E91CE0305794071A149C;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=James Stevens;PARTSTAT=NEEDS-ACTION:mailto:james.stevens@o\n" +
        " racle.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E9359406791C763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=FALSE;CN=Simon Vaillancourt;PARTSTAT=ACCEPTED:mailto:simon.vaillan\n" +
        " court@oracle.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E9359406791D763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Bernard Desruisseaux;PARTSTAT=NEEDS-ACTION:mailto:bernard.\n" +
        " desruisseaux@oracle.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E9359406791E763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Mario Bonin;PARTSTAT=NEEDS-ACTION:mailto:mario.bonin@oracl\n" +
        " e.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E9359406791F763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Jeremy Chone;PARTSTAT=NEEDS-ACTION:mailto:jeremy.chone@ora\n" +
        " cle.com\n" +
        "ATTENDEE;X-ORACLE-PERSONAL-COMMENT-ISDIRTY=TRUE;X-ORACLE-GUID=E9359406792\n" +
        " 0763EE0305794071A39A4;CUTYPE=INDIVIDUAL;RSVP=TRUE;CN=Mike Shaver;PARTSTA\n" +
        " T=NEEDS-ACTION:mailto:mike.x.shaver@oracle.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E93594067921763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=David Ball;PARTSTAT=NEEDS-ACTION:mailto:david.ball@oracle.\n" +
        " com\n" +
        "ATTENDEE;X-ORACLE-GUID=E93594067922763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Marten Haring;PARTSTAT=NEEDS-ACTION:mailto:marten.den.hari\n" +
        " ng@oracle.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E93594067923763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Peter Egyed;PARTSTAT=NEEDS-ACTION:mailto:peter.egyed@oracl\n" +
        " e.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E93594067924763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Francois Perrault;PARTSTAT=NEEDS-ACTION:mailto:francois.pe\n" +
        " rrault@oracle.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E93594067925763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Vladimir Vukicevic;PARTSTAT=NEEDS-ACTION:mailto:vladimir.v\n" +
        " ukicevic@oracle.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E93594067926763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Cyrus Daboo;PARTSTAT=NEEDS-ACTION:mailto:daboo@isamet.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E93594067927763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Lisa Dusseault;PARTSTAT=NEEDS-ACTION:mailto:lisa@osafounda\n" +
        " tion.org\n" +
        "ATTENDEE;X-ORACLE-GUID=E93594067928763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Dan Mosedale;PARTSTAT=NEEDS-ACTION:mailto:dan.mosedale@ora\n" +
        " cle.com\n" +
        "ATTENDEE;X-ORACLE-GUID=E93594067929763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n" +
        " ;RSVP=TRUE;CN=Stuart Parmenter;PARTSTAT=NEEDS-ACTION:mailto:stuart.parme\n" +
        " nter@oracle.com\n" +
        "END:VEVENT\n" +
        "END:VCALENDAR\n\n";

    let event = createEventFromIcalString(ics_xmas);
    let expectedProps = {
        title: "Christmas",
        id: "20041119T052239Z-1000472-1-5c0746bb-Oracle",
        priority: 0,
        status: "CONFIRMED"
    };
    checkProps(expectedProps, event);
    checkRoundtrip(expectedProps, event);

    // Checking start date
    expectedProps = {
        month: 10,
        day: 25,
        year: 2004,
        isDate: true
    };

    checkProps(expectedProps, event.startDate);
    checkProps(expectedProps, event.endDate);

    // Test for roundtrip of x-properties
    let ics_xprop = "BEGIN:VEVENT\n" +
                    "UID:1\n" +
                    "DTSTART:20070521T100000Z\n" +
                    "X-MAGIC:mymagicstring\n" +
                    "END:VEVENT";

    event = createEventFromIcalString(ics_xprop);

    expectedProps = {
        "x-magic": "mymagicstring"
    };

    checkRoundtrip(expectedProps, event);

}

function test_folding() {
    // check folding
    const id = "loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong-id-provoking-folding";
    let todo = cal.createTodo(), todo_ = cal.createTodo();
    todo.id = id;
    todo_.icalString = todo.icalString;
    do_check_eq(todo.id, todo_.id);
    do_check_eq(todo_.icalComponent.getFirstProperty("UID").value, id);
}


function test_icalProps() {
    checkIcalProp("ATTACH", cal.createAttachment());
    checkIcalProp("ATTENDEE", cal.createAttendee());
    checkIcalProp("RELATED-TO", cal.createRelation());
}

/*
 * Helper functions
 */

function checkIcalProp(aPropName, aObj) {
    let icssvc = cal.getIcsService();
    let prop1 = icssvc.createIcalProperty(aPropName);
    let prop2 = icssvc.createIcalProperty(aPropName);
    prop1.value = "foo";
    prop2.value = "bar";
    prop1.setParameter("X-FOO", "BAR");

    if (aObj.setParameter) {
        aObj.icalProperty = prop1;
        do_check_eq(aObj.getParameter("X-FOO"), "BAR");
        aObj.icalProperty = prop2;
        do_check_eq(aObj.getParameter("X-FOO"), null);
    } else if (aObj.setProperty) {
        aObj.icalProperty = prop1;
        do_check_eq(aObj.getProperty("X-FOO"), "BAR");
        aObj.icalProperty = prop2;
        do_check_eq(aObj.getProperty("X-FOO"), null);
    }
}

function checkProps(expectedProps, obj) {
    for (let key in expectedProps) {
        do_check_eq(obj[key], expectedProps[key]);
    }
}

function checkRoundtrip(expectedProps, obj) {
    for (let key in expectedProps) {
        // Need translation
        let icskey = key;
        switch (key) {
            case "id":
                icskey = "uid";
                break;
            case "title":
                icskey = "summary";
                break;
        }
        do_check_true(obj.icalString.indexOf(icskey.toUpperCase()) > 0);
        do_check_true(obj.icalString.indexOf(expectedProps[key]) > 0);
    }
}
