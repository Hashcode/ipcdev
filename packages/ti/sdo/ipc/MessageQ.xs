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
 *  ======== MessageQ.xs ========
 *
 */

var MultiProc  = null;
var NameServer = null;
var MessageQ   = null;
var GateThread = null;
var Settings   = null;
var SyncSem    = null;

var instCount = 0;  /* use to determine if processing last instance */
var sharedCreateId = new Array();

/*
 *  ======== module$meta$init ========
 */
function module$meta$init(name)
{
    /* Only process during "cfg" phase */
    if (xdc.om.$name != "cfg") {
        return;
    }

    /* Initialize the shared CreateId array */
    var MultiProc = xdc.module('ti.sdo.utils.MultiProc');
    sharedCreateId.length != MultiProc.numProcessors;

    for (var i = 0; i < sharedCreateId.length; i++) {
        sharedCreateId[i] = 1;
    }
}

/*
 *  ======== module$use ========
 */
function module$use()
{
    MessageQ   = this;
    Settings   = xdc.useModule('ti.sdo.ipc.family.Settings');
    List       = xdc.useModule("ti.sdo.utils.List");
    MultiProc  = xdc.useModule("ti.sdo.utils.MultiProc");
    NameServer = xdc.useModule('ti.sdo.utils.NameServer');
    GateThread = xdc.useModule("xdc.runtime.knl.GateThread");
    SyncSem    = xdc.useModule("ti.sysbios.syncs.SyncSem");

    /* Plug the SetupTransportProxy for the MessageQ transport */
    if (MessageQ.SetupTransportProxy == null) {
        try {
            MessageQ.SetupTransportProxy =
                xdc.useModule(Settings.getMessageQSetupDelegate());
        }
        catch (e) {
            /* Plug in the generic shared memory transport */
            MessageQ.SetupTransportProxy =
                xdc.useModule('ti.sdo.ipc.transports.TransportShmSetup');
        }
    }
}

/*
 *  ======== module$static$init ========
 *  Initialize module values.
 */
function module$static$init(mod, params)
{
    /* initialize the NameServer param to be used now or later */
    MessageQ.nameSrvPrms.maxRuntimeEntries = params.maxRuntimeEntries;
    MessageQ.nameSrvPrms.tableSection      = params.tableSection;
    MessageQ.nameSrvPrms.maxNameLen        = params.maxNameLen;

    /*
     *  Get the current number of created static instances of this module.
     *  Note: if user creates a static instance after this point and
     *        expects to use NameServer, this is a problem.
     */
     var instCount = this.$instances.length;

    /* create NameServer here only if no static instances are created */
    if (instCount == 0) {
        mod.nameServer = NameServer.create("MessageQ",
                                           MessageQ.nameSrvPrms);
    }
    else {
        mod.nameServer = null;
    }

    /*
     *  If no growth allowed, pre-allocate the max length
     *  Also pre-allocate if numReservedEntries is not zero.
     *  maxRuntimeEntries < numReservedEntries is caught in validate.
     */
    mod.numQueues = this.$instances.length;
    if (params.maxRuntimeEntries != NameServer.ALLOWGROWTH) {
        mod.numQueues += params.maxRuntimeEntries;
    }
    else if (params.numReservedEntries != 0){
        mod.numQueues += params.numReservedEntries;
    }

    mod.queues.length = mod.numQueues;
    mod.canFreeQueues = false;
    mod.freeHookFxn   = params.freeHookFxn;

    if (params.nameTableGate == null) {
         mod.gate = null;
    }
    else {
        mod.gate = params.nameTableGate;
    }

    var messsageQParams = new this.Params;

    /* Initial the seqNum used for tracing */
    mod.seqNum     = 0;

    /* Set the length of the heaps array */
    mod.numHeaps     = params.numHeaps;
    mod.heaps.length = mod.numHeaps;

    /* Initialize the heaps array to null */
    for (var i = 0; i < mod.heaps.length; i++) {
            mod.heaps[i] = null;
    }

    /*
     *  Sort the static heaps by heap id into the heaps array
     */
    for (var i = 0; i < params.staticHeaps.length; i++) {

        /* Make sure the id is not too big */
        if (params.staticHeaps[i].heapId >= mod.numHeaps) {
            MessageQ.$logError("Out of range heapId (" +
                                params.staticHeaps[i].heapId +
                                "). Max heapId is " + (params.numHeaps - 1) +
                                " (MessageQ.numHeaps - 1).", this);
        }

        /* Make sure the same id is not used twice */
        if (mod.heaps[params.staticHeaps[i].heapId] != null) {
            MessageQ.$logError("Cannot register multiple heaps to heapId " +
                                params.staticHeaps[i].heapId + ".", this);
        }

        mod.heaps[params.staticHeaps[i].heapId] = params.staticHeaps[i].heap;
    }

    /* Set the length of the transport array */
    mod.transports.length = MultiProc.numProcessors;

    /* Initialize all the transports to null */
    for (var i = 0; i < mod.transports.length; i++) {
        mod.transports[i][0] = null;
        mod.transports[i][1] = null;
    }

    /*
     *  Sort the static Transports by processor id into the
     *  transport array
     */
    for (var i = 0; i < params.staticTransports.length; i++) {

        /* Make sure the procId is not too big */
        if (params.staticTransports[i].procId >= MultiProc.numProcessors) {
            MessageQ.$logError("MessageQ Out of range procId (" +
                                params.staticTransports[i].procId +
                                "). Max procId is " +
                                (MultiProc.numProcessors) +
                                " (MultiProc.numProcessors).", this);
        }

        /* Make sure the same id is not used twice */
        if (mod.transports[params.staticTransports[i].procId] != null) {
            MessageQ.$logError("Cannot register multiple transports to one" +
                               " remote processor " +
                               params.staticTransports[i].procId + ".", this);
        }

        mod.transports[params.staticTransports[i].procId] =
            params.staticTransports[i].transport;
    }
}

/*
 *  ======== registerHeapMeta ========
 */
function registerHeapMeta(heap, heapId)
{
    var entry = new this.HeapEntry();
    entry.heap = heap;
    entry.heapId = heapId;
    this.staticHeaps.$add(entry);
}

/*
 *  ======== registerTransportMeta ========
 */
function registerTransportMeta(transport, procId, priority)
{
    var entry = new this.transportEntry();
    entry.transport = transport;
    entry.procId    = procId;
    this.staticTransports.$add(entry);
}

/*
 *  ======== viewInitQueues ========
 */
function viewInitQueues(view, obj)
{
    var Program         = xdc.useModule('xdc.rov.Program');
    var NameServer      = xdc.useModule('ti.sdo.utils.NameServer');
    var SharedRegion    = xdc.useModule('ti.sdo.ipc.SharedRegion');

    /* view.name */
    try {
        view.name = NameServer.getName$view("MessageQ", obj.queue);
    }
    catch(e) {
        Program.displayError(view, "name",
            "Error retrieving name from NameServer: " + e);
    }

    /* view.queueId */
    view.queueId = obj.queue & 0xffff;

}

/*
 *  ======== viewInitMessages ========
 */
function viewInitMessages(view, obj)
{
    var Program = xdc.useModule('xdc.rov.Program');
    var MessageQ = xdc.useModule('ti.sdo.ipc.MessageQ');
    var NameServer = xdc.useModule('ti.sdo.utils.NameServer');

    print("viewInitMessages called");

    /* view.label */
    try {
        view.label = NameServer.getName$view("MessageQ", obj.queue);
    }
    catch(e) {
        Program.displayError(view, "label",
            "Error retrieving name from NameServer: " + e);
        return;
    }

    /* Retrieve the ROV view for the embedded high priority message list. */
    addMsgsFromList(view, obj.highList);

    /* Retrieve the ROV view for the embedded normal priority message list. */
    addMsgsFromList(view, obj.normalList);
}

/*
 *  ======== addMsgsFromList ========
 *  Scans the provided list object and adds the messages on it to the view.
 */
function addMsgsFromList(view, list)
{
    var Program = xdc.useModule('xdc.rov.Program');

    print("Calling addMsgsFromList");

    /* Scan the list to retrieve the addresses of the messages. */
    try {
        var listView = Program.scanObjectView("ti.sdo.utils.List", list,
            "Basic");
    }
    /* If there was a problem scanning the list, report it. */
    catch (e) {
        var msgView = Program.newViewStruct('ti.sdo.ipc.MessageQ', 'Messages');
        /*
         * If there was a problem, report it using the first field, and
         * return.
         */
        Program.displayError(msgView, 'seqNum', String(e));
        view.elements.$add(msgView);
        return;
    }

    try {
        /* Fetch each of the message headers on the list. */
        for each (var addr in listView.elems) {
            print("Trying to call getMsgView for 0x" + Number(addr).toString(16));
            var msgView = getMsgView(Number(addr));

            view.elements.$add(msgView);
        }
    }
    catch (e) {
        /*
         * If there were any problems retrieving the view, add an element
         * to report the error.
         */
        var msgView = Program.newViewStruct('ti.sdo.ipc.MessageQ', 'Messages');
        Program.displayError(msgView, 'seqNum', String(e));
        view.elements.$add(msgView);
    }
}

/*
 *  ======== getMsgView ========
 *  Returns a view structure representing the message at the given address.
 */
function getMsgView(addr)
{
    var Program = xdc.useModule('xdc.rov.Program');
    var MessageQ = xdc.useModule('ti.sdo.ipc.MessageQ');

    print("getting message view: 0x" + Number(addr).toString(16));

    /* Create a new message view to populate. */
    var msgView = Program.newViewStruct('ti.sdo.ipc.MessageQ', 'Messages');

    /* Fetch the message header from the given address. */
    try {
        var msgHeader = Program.fetchStruct(MessageQ.MsgHeader$fetchDesc,
                                            Number(addr));
    }
    catch (e) {
        /*
         * If there was a problem, report it using the first field, and
         * return the view.
         */
        Program.displayError(msgView, 'seqNum', String(e));
        return (msgView);
    }

    /* Display the messages sequence number and size. */
    msgView.seqNum = msgHeader.seqNum;
    msgView.msgSize = msgHeader.msgSize;

    /* The flags field contains the message priority. */
    var priority = msgHeader.flags & MessageQ.PRIORITYMASK;
    getPriority(msgView, priority);

    /* The flags field also contains the trace enabled bit. */
    msgView.traceEnabled = msgHeader.flags & MessageQ.TRACEMASK;

    /* The flags field also contains the version bits. */
    msgView.version = (msgHeader.flags & MessageQ.VERSIONMASK) >> 13;

    /* Display the message ID set by the user. */
    msgView.msgId = msgHeader.msgId;

    /*
     * Scan in the MultiProc view so we have the mapping from processor id
     * to processor name.
     */
    try {
        var multiProcView = Program.scanModuleView('ti.sdo.utils.MultiProc',
                                                   'Module');
    }
    catch (e) {
        /*
         * If there was a problem scanning the MultiProc view, display the
         * error, but keep processing anyway.
         */
        Program.displayError(msgView, 'srcProc', String(e));
    }

    /* Lookup the names of the destination, reply, and source processors. */
    msgView.srcProc = multiProcView.nameList[msgHeader.srcProc];

    /* If a reply queue hasn't been specified, display "N/A" */
    if (msgHeader.replyId == 0xFFFF) {
        msgView.replyProc = "N/A";
        msgView.replyId = "N/A";
    }
    /* Otherwise display the reply queue. */
    else {
        msgView.replyProc = multiProcView.nameList[msgHeader.replyProc];
        // TODO - Should this display the name of the MessageQ?
        msgView.replyId = String(msgHeader.replyId);
    }

    /* Display the heap that the message was allocated from. */
    msgView.heap = getHeapNameFromId(msgHeader.heapId);

    return (msgView);
}

/*
 *  ======== getPriority ========
 *  Fills in the message views 'priority' field given the priority value.
 */
function getPriority(msgView, priority)
{
    var MessageQ = xdc.useModule('ti.sdo.ipc.MessageQ');
    var Program = xdc.useModule('xdc.rov.Program');

    switch (priority) {
        case MessageQ.NORMALPRI:
            msgView.priority = "(" + MessageQ.NORMALPRI + ") Normal";
            break;
        case MessageQ.HIGHPRI:
            msgView.priority = "(" + MessageQ.HIGHPRI + ") High";
            break;
        case MessageQ.RESERVEDPRI:
            msgView.priority = "(" + MessageQ.RESERVEDPRI + ") Reserved";
            break;
        case MessageQ.URGENTPRI:
            msgView.priority = "(" + MessageQ.URGENTPRI + ") Urgent";
            break;
        default:
            Program.displayError(msgView, 'priority',
                                 'Message has invalid priority: ' + priority);
    }
}

/*
 *  ======== getHeapNameFromId ========
 *  This function takes the heap Id from a message (the heap ids are assigned
 *  by MessageQ) and retrieves a name for the heap.
 */
function getHeapNameFromId(heapId)
{
    var Program = xdc.useModule('xdc.rov.Program');

    /* The module state contains the list of heaps. */
    var modView = Program.scanModuleView('ti.sdo.ipc.MessageQ', 'Module');

    /* Get the string representation of the address. */
    if ((heapId == -1) || (heapId == 65535)) {
        var heapAddr = "[static]";
    }
    else {
        var heapAddr = String(modView.heaps[heapId]);
    }

    return (heapId + " (" + heapAddr + ")");
}

/*
 *  ======== viewInitModule ========
 */
function viewInitModule(view, mod)
{
    var Program = xdc.useModule('xdc.rov.Program');

    /* Scan in the array of heaps. */
    try {
        view.heaps = Program.fetchArray(mod.heaps$fetchDesc, mod.heaps,
                                          mod.numHeaps);
    }
    catch (e) {
        Program.displayError(view, 'heaps', "Problem retrieving array of " +
                             "heaps: " + e);
    }

    /* Display the handle to the module's gate. */
    view.gate = mod.gate;

    /*
     * Display the module's sequence number, which is the next value
     * that will be used.
     */
    view.nextSeqNum = mod.seqNum;

    /* Display the freeHookFxn if there is one. */
    if (Number(mod.freeHookFxn) != 0 ) {
        view.freeHookFxn = Program.lookupFuncName(Number(mod.freeHookFxn));
    }
}

function module$validate()
{
    if ((MessageQ.maxRuntimeEntries != NameServer.ALLOWGROWTH) &&
        (MessageQ.maxRuntimeEntries < MessageQ.numReservedEntries)) {
        MessageQ.$logFatal(
            "If MessageQ.maxRuntimeEntries is not NameServer.ALLOWGROWTH, " +
            "it cannot be less than MessageQ.numReservedEntries.",
            MessageQ);
    }
}
