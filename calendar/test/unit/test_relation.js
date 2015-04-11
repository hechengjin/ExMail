/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
    // Create Relation
    let r1 = cal.createRelation();

    // Create Items
    let e1 = cal.createEvent();
    let e2 = cal.createEvent();

    // Testing relation set/get.
    let properties = {
        relType: "PARENT",
        relId: e2.id
    }

    for (let [property, value] in Iterator(properties)) {
        r1[property] = value;
        do_check_eq(r1[property], value);
    }

    // Add relation to event
    e1.addRelation(r1);

    // Add 2nd relation to event.
    let r2 = cal.createRelation();
    r2.relId = "myid2";
    e1.addRelation(r2);

    // Check the item functions
    checkRelations(e1, [r1, r2]);

    // modify the Relations
    modifyRelations(e1, [r1, r2]);

    // test icalproperty
    r2.icalProperty;
}

function checkRelations(event, expRel) {
    let countObj = {};
    let allRel = event.getRelations(countObj);
    do_check_eq(countObj.value, allRel.length);
    do_check_eq(allRel.length, expRel.length);

    // check if all expacted relations are found
    for (let i = 0; i < expRel.length; i++) {
        do_check_neq(allRel.indexOf(expRel[i]), -1);
    }

    // Check if all found relations are expected
    for (let i = 0; i < allRel.length; i++) {
        do_check_neq(expRel.indexOf(allRel[i]), -1);
    }
}

function modifyRelations(event, oldRel) {
    let allRel = event.getRelations({});
    let rel = allRel[0];

    // modify the properties
    rel.relType = "SIBLING";
    do_check_eq(rel.relType, "SIBLING");
    do_check_eq(rel.relType, allRel[0].relType);

    // remove one relation
    event.removeRelation(rel);
    do_check_eq(event.getRelations({}).length, oldRel.length - 1);

    // add one relation and remove all relations
    event.addRelation(oldRel[0]);
    event.removeAllRelations();
    do_check_eq(event.getRelations({}), 0);
}
