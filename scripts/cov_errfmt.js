#!/usr/bin/node
/** Report Coverity errors in coverity errors JSON file (produced with cov-format-errors ...)
* in Markdown format.
* # Usage
* 
* Pass coverity errors (JSON) file as first command line argument.
* ```
* node cov_errfmt.js cov_errors_v8.json
* ```
* 
* # Source Code Files
* 
* If you have the source code present, the utility is able to cross reference the source
* code and show it in the report. Set env variable SRC_PRESENT=1 to have utility look up
* the source files.
* Note: Source code presnt in current dir must be in sync with files at the time or Cov run.

*/
//var webview = require("./jsx/webview.js");
var fs = require("fs");
var path = require('path');

if (!process.argv[2]) { console.error("Need filename of Cov. JSON error report !"); process.exit(1);}
// Have source code present ? TODO: Allow passing from CLI
var havesrc = process.env["SRC_PRESENT"];
var fn = process.argv[2];
fn = path.resolve(fn);
var err_rpt = require(fn);

var errs = err_rpt.issues;
if (!Array.isArray(errs)) { throw "Errors not in array !"; }
var fcache = {}; // File Cache (See fcache_add())


function report_md(errs) {
var cont = "# Coverity issues v."+err_rpt.formatVersion+"\n\n";
errs.forEach((re) => {
  
  var e = {}; // Brief keys
  // Also "type", "subtype"
  var fname = re.strippedMainEventFilePathname;
  // var func = re. // Only events have this
  var ckr = re.checkerName;
  var lnum = re.mainEventLineNumber;
  var func = re.functionDisplayName; // Can be null (!)
  var evs = re.events;
  if (!evs) { throw "No events for defect"; }
  var line;
  if (havesrc) {
    var err = fcache_add(fname);
    if (err) { console.log("Err: "+err+ " caching "+fname+""); return undefined; }
    line = fcache[fname][lnum-1]; // Adjust to idx
  }
  cont += "- "+ckr+" - File: "+fname+", func: "+func+"\n";
  //cont += "   - top-evs:"+evs.length+"\n";
  if (havesrc) { cont += "   - Line #"+lnum+": \""+line+"\"\n"; }
  
  var opts = {nameattr: "pid", idattr: "pid", licb: licb, liclX: "", debug: 0};
  // Events (TODO: Use ...):
  // var evout = webview.navimenu2l(evs, opts); cont += evout;
  showevs(evs, 0);
  function showevs(evs, lvl) {
  evs.forEach((iev) => {
    var i = iev.eventTreePosition; // e.g. "2", "4.1" (in depth)
    var desc = iev.eventDescription;
    var fname = iev.strippedFilePathname;
    var lnum  = iev.lineNumber;
    var evtag = iev.eventTag;
    // Min. 4 sp. (+2 att each leve > 0)
    var pad = "".padEnd(4+lvl*2);
    cont += "    "+pad+"- "+i +" "+fname+", l."+lnum+": "+desc+" ("+evtag+")\n";
    //OLD: cont += "      - EvTag:"+evtag+"\n";
    if (iev.events) { showevs(iev.events, lvl+1); }
  });
  } // showevs
});
  function licb(iev) {
    //return "<span data-pid=\""+p.pid+"\" class=\"pidlink\">"+p.pid+"</span> "+p.cmd + " ("+p.owner+")\n";
    return "      - "+iev.eventTreePosition+" "+iev.strippedFilePathname+", l."+iev.lineNumber+": "+iev.eventDescription+"("+iev.eventTag+")\n";
  }
  return cont;
}

var cont = report_md(errs);
console.log(cont);

/** Cache file into in-mem pool of files as same file may be (is usually) referred to
* many times from the error report.
*/
function fcache_add(fname) {
  if (fcache[fname]) { return 0; } // Don't redo
  // try {
  var data = fs.readFileSync(fname, 'utf8');
  // } catch (ex) { console.error("No file '"+fname+"' present"); }
  if (!data) { return 1; }
  fcache[fname] = data.split(/\n/);
  if (!fcache[fname].length) { return 2; }
  return 0;
}
