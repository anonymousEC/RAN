
package org.apache.hadoop.hdfs.server.datanode.erasurecode;

import java.io.IOException;
import java.nio.ByteBuffer;

import org.apache.hadoop.classification.InterfaceAudience;
import org.apache.hadoop.hdfs.server.datanode.DataNodeFaultInjector;
import org.apache.hadoop.hdfs.server.datanode.metrics.DataNodeMetrics;
import org.apache.hadoop.util.Time;

// RAN-begin
import org.apache.hadoop.hdfs.protocol.DatanodeInfo;
import java.util.BitSet;
import java.io.*;
import java.net.*;
// RAN-end

/**
 * RANReconstructor, similar to StripedBlockReconstructor
 * support two following mode
 * 1. chunk reconstructor: reconstruct one striped block of one node in the
 * striped block group
 * 2. node reconstructor: reconstruct all striped block of one node in the
 * striped block group
 * 
 * support four repair method: tradition RP NetEC RAN
 * 
 * For the convenience of testing, the use of this integration must meet
 * restrictions. Since they do not affect the test results, they only increase
 * the flexibility of the integration. They will be solved and developeds later.
 * 1. Fixed: encoding strategy, chunksize, cellsize. If you need to modify, you
 * should also modify the configuration of RAN
 * 2. the size of repaired file = Multiples of k * cellsize
 * 3. only one node failure
 */
@InterfaceAudience.Private
class RANReconstructor extends StripedReconstructor
        implements Runnable {

    private StripedWriter stripedWriter;
    // RAN-begin
    private final DatanodeInfo[] targets;
    private final short[] targetIndices;
    private int targetNum = 0;
    private String[] lostBlockNames = null;
    private String[] lostBlockPoolNames = null;

    private int chunkNodeFlag = 0;// 0-init,1-chun,2-node,-1-error
    private int methodFlag = 0;// 0-tradition,1-RP, 2-NetEC, 3-RAN
    private String coordinatorIP = null;
    private int coordinatorPort = 8400;
    private Socket socket = null;
    private DataOutputStream orOut;
    private DataInputStream orIn;
    // RAN-end

    RANReconstructor(ErasureCodingWorker worker,
            StripedReconstructionInfo stripedReconInfo) {
        super(worker, stripedReconInfo);
        
        // RAN-begin
        this.targets = stripedReconInfo.getTargets();
        assert targets != null;
        targetIndices = new short[targets.length];
        initTargetIndices(stripedReconInfo);
        this.targetNum = targetIndices.length;
        this.lostBlockNames = new String[targetNum];
        this.lostBlockPoolNames = new String[targetNum];
        for (int i = 0; i < targetNum; i++) {// get falied blockname
            this.lostBlockNames[i] = getBlock(targetIndices[i]).getBlockName();
            this.lostBlockPoolNames[i] = getBlock(targetIndices[i]).getBlockPoolId();
        }
        this.chunkNodeFlag = getConf().getInt("dfs.datanode.ec.RAN.chunknode", 0);
        this.methodFlag = getConf().getInt("dfs.datanode.ec.RAN.method", 0);
        this.coordinatorIP = getConf().get("RAN.coordinator.ip");
        this.coordinatorPort = getConf().getInt("RAN.coordinator.port", 8400);
        try {
            this.socket = new Socket(coordinatorIP, coordinatorPort);
            this.orOut = new DataOutputStream(socket.getOutputStream());
            this.orIn = new DataInputStream(socket.getInputStream());
        } catch (Throwable e) {
            LOG.info("new Socket-new DataOutputStream-new DataInputStream");
        }
        // RAN-end
    }

    @Override
    public void run() {
        try {
            long startUs = Time.monotonicNowNanos() / 1000;
            reconstruct();
            long writeEndUs = Time.monotonicNowNanos() / 1000;
            long totalReconstruct = writeEndUs - startUs;
            LOG.info("RAN[RANReconstructor]: chunkNodeFlag: " + chunkNodeFlag);
            LOG.info("RAN[RANReconstructor]: methodFlag: " + methodFlag);
            LOG.info("RAN[RANReconstructor]: lostBlockNames[0]: " + lostBlockNames[0]);
            LOG.info("RAN[RANReconstructor]: RAN ALLRepairTotalTime: [" + totalReconstruct
                    + "us]  ["
                    + totalReconstruct / 1000 + "ms]");

            // Currently we don't check the acks for packets, this is similar as
            // block replication.
        } catch (Throwable e) {
            LOG.warn("Failed to reconstruct striped block: {}", getBlockGroup(), e);
            getDatanode().getMetrics().incrECFailedReconstructionTasks();
        } finally {
            getDatanode().decrementXmitsInProgress(getXmits());
            cleanup();
        }
    }

    @Override
    void reconstruct() throws IOException {
        try {
            orOut.writeInt(-1);// reconstruct cmd
            orOut.writeInt(chunkNodeFlag);
            orOut.writeInt(methodFlag);
            byte[] messageBytes = lostBlockNames[0].getBytes("UTF-8");// only one node failure
            orOut.writeInt(messageBytes.length);
            orOut.write(messageBytes);
            orOut.flush();

            int completeFlag = orIn.readInt();
            if (completeFlag == 1)
                LOG.info("RAN[RANReconstructor]: Repair Completed!" + lostBlockNames[0]);
            else
                LOG.info("RAN[RANReconstructor]: Repair NOT Completed!" + lostBlockNames[0]);
            socket.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void initTargetIndices(StripedReconstructionInfo stripedReconInfo) {
        int dataBlkNum = stripedReconInfo.getEcPolicy().getNumDataUnits();
        int parityBlkNum = stripedReconInfo.getEcPolicy().getNumParityUnits();
        BitSet bitset = getLiveBitSet();
        int m = 0;
        for (int i = 0; i < dataBlkNum + parityBlkNum; i++)
            if (!bitset.get(i))
                if (getBlockLen(i) > 0)
                    if (m < targets.length)
                        targetIndices[m++] = (short) i;
    }
}
