const BIN_SUFFIX = ".exe";

var ac = Components.classes["@mozilla.org/steel/application;1"]
          .getService(Components.interfaces.steelIApplication).console;
					
function createUpdateMar(){
	
	let outMAR = do_get_file("../../../installer-stage/out.mar", true);

	// Create the actual MAR file.
	createMAR(outMAR, do_get_file("../../../installer-stage/core.work"), ["AccessibleMarshal.dll", "application.ini", "blocklist.xml"]);
	
}



function createMAR(outMAR, dataDir, files) {

  // Get an nsIProcess to the signmar binary.
  let process = Cc["@mozilla.org/process/util;1"].
                createInstance(Ci.nsIProcess);
	//signmarBin 这个是什么程序？？
  let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

  // Make sure the signmar binary exists and is an executable.
  //do_check_true(signmarBin.exists());
  //do_check_true(signmarBin.isExecutable());

  // Setup the command line arguments to create the MAR.
  let args = ["-C", dataDir.path, "-H", "\@MAR_CHANNEL_ID\@", 
              "-V", "13.0a1", "-c", outMAR.path];
  args = args.concat(files);

  ac.log('Running: ' + signmarBin.path);
  process.init(signmarBin);
  process.run(true, args, args.length);

  // Verify signmar returned 0 for success.
  //do_check_eq(process.exitValue, 0);

  // Verify the out MAR file actually exists.
  //do_check_true(outMAR.exists());
}

function do_get_file(path, allowNonexistent) {
  try {
    let lf = Components.classes["@mozilla.org/file/directory_service;1"]
      .getService(Components.interfaces.nsIProperties)
      .get("CurWorkD", Components.interfaces.nsILocalFile);
			
    let bits = path.split("/");
    for (let i = 0; i < bits.length; i++) {
      if (bits[i]) {
        if (bits[i] == "..")
          lf = lf.parent;
        else
          lf.append(bits[i]);
      }
    }

    if (!allowNonexistent && !lf.exists()) {
      // Not using do_throw(): caller will continue.
      _passed = false;
      var stack = Components.stack.caller;
      _dump("TEST-UNEXPECTED-FAIL | " + stack.filename + " | [" +
            stack.name + " : " + stack.lineNumber + "] " + lf.path +
            " does not exist\n");
    }
		//ac.log(lf.leafName) //out.mar
		//ac.log(lf.path)//E:\github\sync\Firemail\ExMail\obj_Firemail\installer-stage\out.mar
    return lf;
  }
  catch (ex) {
    ac.log(ex.toString(), Components.stack.caller);
  }

  return null;
}