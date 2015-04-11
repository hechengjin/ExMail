/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Helper function.
 */
function newUint8Worker() {
  let worker = newWorker();
  let index = 0; // index for read
  let buf = [];

  worker.Buf.writeUint8 = function (value) {
    buf.push(value);
  };

  worker.Buf.readUint8 = function () {
    return buf[index++];
  };

  worker.Buf.seekIncoming = function (offset) {
    index += offset;
  };

  worker.debug = do_print;

  return worker;
}
/**
 * Verify GsmPDUHelper#readICCUCS2String()
 */
add_test(function test_read_icc_ucs2_string() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;

  // 0x80
  let text = "TEST";
  helper.writeUCS2String(text);
  // Also write two unused octets.
  let ffLen = 2;
  for (let i = 0; i < ffLen; i++) {
    helper.writeHexOctet(0xff);
  }
  do_check_eq(helper.readICCUCS2String(0x80, (2 * text.length) + ffLen), text);

  // 0x81
  let array = [0x08, 0xd2, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0xca,
               0xff, 0xff];
  let len = array.length;
  for (let i = 0; i < len; i++) {
    helper.writeHexOctet(array[i]);
  }
  do_check_eq(helper.readICCUCS2String(0x81, len), "Mozilla\u694a");

  // 0x82
  let array2 = [0x08, 0x69, 0x00, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61,
                0xca, 0xff, 0xff];
  let len2 = array2.length;
  for (let i = 0; i < len2; i++) {
    helper.writeHexOctet(array2[i]);
  }
  do_check_eq(helper.readICCUCS2String(0x82, len2), "Mozilla\u694a");

  run_next_test();
});

/**
 * Verify GsmPDUHelper#read8BitUnpackedToString
 */
add_test(function test_read_8bit_unpacked_to_string() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
  let buf = worker.Buf;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

  // Only write characters before PDU_NL_EXTENDED_ESCAPE to simplify test.
  for (let i = 0; i < PDU_NL_EXTENDED_ESCAPE; i++) {
    helper.writeHexOctet(i);
  }

  // Also write two unused fields.
  let ffLen = 2;
  for (let i = 0; i < ffLen; i++) {
    helper.writeHexOctet(0xff);
  }

  do_check_eq(helper.read8BitUnpackedToString(PDU_NL_EXTENDED_ESCAPE + ffLen),
              langTable.substring(0, PDU_NL_EXTENDED_ESCAPE));
  run_next_test();
});


