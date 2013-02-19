/*
 * Copyright (c) 2012-2013, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
*  ======== getFileContents ========
*  Returns a string containing the contents of file at 'filename'
*/

var mod = "GateMP";
function getFileContents(filename)
{
    var symbolDB = [];

    try {
        var input = "";
        var inputFile = java.io.File(filename);
        var fos = java.io.FileReader(inputFile);
        var reader = new java.io.BufferedReader(fos);
        var line;
        while ((line = reader.readLine()) != null) {
            var matched = line.match("(" + mod + "_[^ ,().:\t;\[{}]+)");
            if (matched != null && symbolDB.indexOf(matched[1]) == -1) {
                if (matched[1].indexOf("__") != -1) {
                    continue;
                }
                symbolDB.push(matched[1]);
            }
        }

        reader.close();
    }
    catch (e) {
        print(e);
        throw("Error reading file '" + filename + "'");
    }



    return (symbolDB);
}

function getVal(str)
{
    if (str.indexOf("_S_") != -1) {
        return (0);
    }
    else if (str.indexOf("_E_") != -1) {
        return (1);
    }
    else if (str.match(mod + "_([A-Z_]+[^a-z])")) {
        return (2);
    }
    else if (str != (mod + "_Params_init") && str.match(mod + "_([A-Z])")) {
        return (3);
    }
    else {
        return (4);
    }
}

/*
 *  ======== sortFunction ========
 */
function sortFunction(a, b)
{
    var valA = getVal(a);
    var valB = getVal(b);

    if (valA != valB) {
        return (valA - valB);
    }
    else {
        return (a > b);
    }
}

/*
 *  ======== strPad ========
 */
function strPad(str, pad)
{
    for (i = str.length; i < pad; i++) {
        str += " ";
    }
    return(str);
}


var results = getFileContents(mod + ".h");
results.sort(sortFunction);
for each (var symbol in results) {
    print("    #pragma FUNC_EXT_CALLED(" + symbol + ");");
}
