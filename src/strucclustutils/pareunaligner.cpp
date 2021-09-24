
#include <string>
#include <vector>
#include <sstream>
#include <tmalign/TMalign.h>
#include "DBReader.h"
#include "DBWriter.h"
#include "Debug.h"
#include "Util.h"
#include "QueryMatcher.h"
#include "LocalParameters.h"
#include "Matcher.h"
#include "PareunAlign.h"

#ifdef OPENMP
#include <omp.h>
#endif

#include <math.h>

// static two dimensional array instead of vectors
vector<int> findNearestNeighbour(char * nn, Coordinates & ca, int length, char *sseq, Sequence & seqAll){
    // (char*)mem_align(ALIGN_INT, par.maxSeqLen * sizeof(char) * 4 );

    // calculate pairwise matrix (euclidean distance)
    std::vector<vector<float>> nnDist;
    std::vector<vector<int>> nnPos;
    std::vector<int> nnInt;
    float seqDist;

    for(int i = 0; i < length; i++){
        std::vector<std::pair<float, int>> seqDistList;
        for(int j = 0; j < length; j++){
            if(i != j) {
                // calculate distance
                seqDist = sqrt(
                        ((ca.x[i] - ca.x[j]) * (ca.x[i] - ca.x[j])) + ((ca.y[i] - ca.y[j]) * (ca.y[i] - ca.y[j])) +
                        ((ca.z[i] - ca.z[j]) * (ca.z[i] - ca.z[j])));
                seqDistList.push_back(make_pair(seqDist, j));
            }
        }
        // find the four nearest neighbours for each amino acid
        std::sort(seqDistList.begin(), seqDistList.end());
        int pos[4];
        for(int m  = 0; m < 4; m++){
            pos[m]  = seqDistList[m].second;
        }
        std::sort(begin(pos), end(pos));
        for(int n = 0; n < 4; n++){
//            cout << sseq[pos[n]] << endl;
//            nn[i*4 + n]  = sseq[pos[n]];
            nnInt.push_back(seqAll.numSequence[pos[n]]);
        }
        seqDistList.clear();

//        for(int k = 0;   k < seqDistList.size(); k++){
//            cout << seqDistList[k].first << " : " << seqDistList[k].second << endl;
//        }
//        for(int l = 0; l < 4; l++){ cout << pos[l] <<  endl;}
//        cout << "--------" << endl;
    }

//    for(int a = 0; a < 4*nnInt.size(); a++){
//        cout << nnInt[a] << endl;
//    }

    return nnInt;
}

int needlemanWunschScore(int subQNNi[4], int subTNNi[4], SubstitutionMatrix *subMat){

    int nwGapPenalty = 2;

    vector<int> revSeq, zeroVector;
    vector<vector<int> > scoringMatrix;
    int nextValue[3];

    // initialize scoring matrix
    for(int i = 0; i <= 4; i++) {
        // vector of length seqLength + 1 with 0-entries
        zeroVector.push_back(0);
    }

    for(int j = 0; j <= 4; j++){
        scoringMatrix.push_back(zeroVector);
    }

    for(int i = 1; i < 5; i++){
        for(int j = 1; j < 5; j++){
            // calculate scores - bonus for base pairing and penalty for not
            scoringMatrix[i][j] = std::max(scoringMatrix[i-1][j-1] + subMat->subMatrix[subQNNi[i - 1]][subTNNi[j - 1]], scoringMatrix[i-1][j] - nwGapPenalty);
            scoringMatrix[i][j] = std::max(scoringMatrix[i][j-1] - nwGapPenalty, scoringMatrix[i][j]);
        }
    }

    // for testing: print matrix
//    cout << "Scoring Matrix" << endl;
//    for (int a = 0; a < scoringMatrix.size(); a++) {
//        for(int b = 0; b < scoringMatrix[0].size(); b++){
//            cout << scoringMatrix[a][b] << " ";
//        }
//        cout << endl;
//    }
    int score = scoringMatrix[4][4] / 4;

    return score;
}

Matcher::result_t alignByNN(char * querynn, vector<int> nnIntQuery, char *querySSeq, int queryLen, char * targetnn, vector<int> nnIntTarget, char *targetSSeq, int targetLen, SubstitutionMatrix *subMat, Sequence & qSeq, Sequence & tSeq, int gapOpen, int gapExtern, EvalueComputation & evaluer){

    Matcher::result_t result;
    // gotoh sw itself

        struct scores{ short H, E, F; };
        uint16_t max_score = 0;
        char query[4], target[4];
        scores *workspace = new scores[queryLen * 2 + 2];
        scores *curr_sHEF_vec = &workspace[0];
        scores *prev_sHEF_vec = &workspace[queryLen + 1];
        char subQNN[4], subTNN[4];
        int subTNNi[4], subQNNi[4];
        // top row need to be set to a 0 score
        memset(prev_sHEF_vec, 0, sizeof(scores) * (queryLen + 1));
        for (int i = 0; i < targetLen; i++) {

            for(int a=0; a <4; a++){ subTNNi[a]=nnIntTarget[4*i + a];};
            // left outer column need to be set to a 0 score
            prev_sHEF_vec[0].H = 0; prev_sHEF_vec[0].E = 0; prev_sHEF_vec[0].F = 0;
            curr_sHEF_vec[0].H = 0; curr_sHEF_vec[0].E = 0; curr_sHEF_vec[0].E = 0;
            for (int j = 1; j <= queryLen; j++) {
                for(int a=0; a <4; a++){subQNNi[a]=nnIntQuery[4*(j-1) + a];}
                curr_sHEF_vec[j].E = std::max(curr_sHEF_vec[j - 1].H - gapOpen, curr_sHEF_vec[j - 1].E - gapExtern); // j-1
                curr_sHEF_vec[j].F = std::max(prev_sHEF_vec[j].H - gapOpen, prev_sHEF_vec[j].F - gapExtern); // i-1
                int bla = needlemanWunschScore(subTNNi, subQNNi, subMat);
                const short tempH = prev_sHEF_vec[j - 1].H + subMat->subMatrix[tSeq.numSequence[i]][qSeq.numSequence[j - 1]] + bla; // i - 1, j - 1
                curr_sHEF_vec[j].H = std::max(tempH, curr_sHEF_vec[j].E);
                curr_sHEF_vec[j].H = std::max(curr_sHEF_vec[j].H, curr_sHEF_vec[j].F);
                curr_sHEF_vec[j].H = std::max(curr_sHEF_vec[j].H, static_cast<short>(0));
                max_score = static_cast<uint16_t>(std::max(static_cast<uint16_t>(curr_sHEF_vec[j].H), max_score));
            }
            std::swap(prev_sHEF_vec, curr_sHEF_vec);
        }
        delete [] workspace;

//        cout << "max score: " << max_score << endl;
//    result.backtrace = optAlnResult.cigar_string;
    result.score = max_score;
//    result.qStartPos = optAlnResult.query_start;
//    result.qEndPos = optAlnResult.query_end;
//    result.dbEndPos = optAlnResult.target_end;
//    result.dbStartPos = optAlnResult.query_start;
    //result.qCov = SmithWaterman::computeCov(result.qStartPos1, result.qEndPos1, querySeqObj->L);
//    result.qcov = SmithWaterman::computeCov(result.qStartPos, result.qEndPos, qSeq.L);
    //result.tCov = SmithWaterman::computeCov(result.dbStartPos1, result.dbEndPos1, targetSeqObj->L);
//    result.dbcov = SmithWaterman::computeCov(result.dbStartPos, result.dbEndPos, tSeq.L);
    result.eval = evaluer.computeEvalue(result.score, queryLen);


    return result;
}


int pareunaligner(int argc, const char **argv, const Command& command) {
    LocalParameters &par = LocalParameters::getLocalInstance();
    par.parseParameters(argc, argv, command, false, 0, MMseqsParameter::COMMAND_ALIGN);

    Debug(Debug::INFO) << "Sequence database: " << par.db1 << "\n";
    DBReader<unsigned int> qdbr((par.db1+"_ss").c_str(), (par.db1+"_ss.index").c_str(), par.threads, DBReader<unsigned int>::USE_DATA|DBReader<unsigned int>::USE_INDEX);
    qdbr.open(DBReader<unsigned int>::NOSORT);

    DBReader<unsigned int> qcadbr((par.db1+"_ca").c_str(), (par.db1+"_ca.index").c_str(), par.threads, DBReader<unsigned int>::USE_DATA|DBReader<unsigned int>::USE_INDEX);
    qcadbr.open(DBReader<unsigned int>::NOSORT);

    SubstitutionMatrix subMat(par.scoringMatrixFile.aminoacids, 2.0, par.scoreBias);

    DBReader<unsigned int> *tdbr = NULL;
    DBReader<unsigned int> *tcadbr = NULL;

    bool touch = (par.preloadMode != Parameters::PRELOAD_MODE_MMAP);

    bool sameDB = false;
    if (par.db1.compare(par.db2) == 0) {
        sameDB = true;
        tdbr = &qdbr;
        tcadbr = &qcadbr;
    } else {
        tdbr = new DBReader<unsigned int>((par.db2+"_ss").c_str(), (par.db2+"_ss.index").c_str(), par.threads, DBReader<unsigned int>::USE_DATA|DBReader<unsigned int>::USE_INDEX);
        tdbr->open(DBReader<unsigned int>::NOSORT);
        tcadbr = new DBReader<unsigned int>((par.db2+"_ca").c_str(), (par.db2+"_ca.index").c_str(), par.threads, DBReader<unsigned int>::USE_DATA|DBReader<unsigned int>::USE_INDEX);
        tcadbr->open(DBReader<unsigned int>::NOSORT);
        if (touch) {
            tdbr->readMmapedDataInMemory();
            tcadbr->readMmapedDataInMemory();
        }
    }

    Debug(Debug::INFO) << "Result database: " << par.db3 << "\n";
    DBReader<unsigned int> resultReader(par.db3.c_str(), par.db3Index.c_str(), par.threads, DBReader<unsigned int>::USE_DATA|DBReader<unsigned int>::USE_INDEX);
    resultReader.open(DBReader<unsigned int>::LINEAR_ACCCESS);
    Debug(Debug::INFO) << "Output file: " << par.db4 << "\n";
    DBWriter dbw(par.db4.c_str(), par.db4Index.c_str(), static_cast<unsigned int>(par.threads), par.compressed,  Parameters::DBTYPE_ALIGNMENT_RES);
    dbw.open();

    //temporary output file
    Debug::Progress progress(resultReader.getSize());

#pragma omp parallel
    {

        unsigned int thread_idx = 0;
#ifdef OPENMP
        thread_idx = static_cast<unsigned int>(omp_get_thread_num());
#endif
        float * query_x = (float*)mem_align(ALIGN_FLOAT, par.maxSeqLen * sizeof(float) );
        float * query_y = (float*)mem_align(ALIGN_FLOAT, par.maxSeqLen * sizeof(float) );
        float * query_z = (float*)mem_align(ALIGN_FLOAT, par.maxSeqLen * sizeof(float) );
        float * target_x = (float*)mem_align(ALIGN_FLOAT, par.maxSeqLen * sizeof(float) );
        float * target_y = (float*)mem_align(ALIGN_FLOAT, par.maxSeqLen * sizeof(float) );
        float * target_z = (float*)mem_align(ALIGN_FLOAT, par.maxSeqLen * sizeof(float) );
//        float *mem = (float*)mem_align(ALIGN_FLOAT,6*par.maxSeqLen*4*sizeof(float));
        Sequence qSeq(par.maxSeqLen, Parameters::DBTYPE_AMINO_ACIDS, (const BaseMatrix *) &subMat, 0, false, par.compBiasCorrection);
        Sequence tSeq(par.maxSeqLen, Parameters::DBTYPE_AMINO_ACIDS, (const BaseMatrix *) &subMat, 0, false, par.compBiasCorrection);
        int * ires = new int[par.maxSeqLen];
        char * querynn  = (char*)mem_align(ALIGN_INT, par.maxSeqLen * sizeof(char) * 4 );
        char * targetnn = (char*)mem_align(ALIGN_INT, par.maxSeqLen * sizeof(char) * 4 );

        std::vector<Matcher::result_t> alignmentResult;
        PareunAlign paruenAlign(par.maxSeqLen, &subMat); // subMat called once, don't need to call it again?
        char buffer[1024+32768];
        std::string resultBuffer;
        //int gapOpen = 15; int gapExtend = 3; // 3di gap optimization
        EvalueComputation evaluer(tdbr->getAminoAcidDBSize(), &subMat, par.gapOpen.aminoacids, par.gapExtend.aminoacids);
        // write output file

#pragma omp for schedule(dynamic, 1)
        for (size_t id = 0; id < resultReader.getSize(); id++) {
            progress.updateProgress();
            char *data = resultReader.getData(id, thread_idx);
            size_t queryKey = resultReader.getDbKey(id);
            if(*data != '\0') {
                unsigned int queryId = qdbr.getId(queryKey);
                char *querySeq = qdbr.getData(queryId, thread_idx);
                unsigned int querySeqLen = qdbr.getSeqLen(id);
                qSeq.mapSequence(id, queryKey, querySeq, querySeqLen);

                int queryLen = static_cast<int>(qdbr.getSeqLen(queryId));
                float *qdata = (float *) qcadbr.getData(queryId, thread_idx);
                Coordinates queryCaCords;
                memcpy(query_x, qdata, sizeof(float) * queryLen);
                memcpy(query_y, &qdata[queryLen], sizeof(float) * queryLen);
                memcpy(query_z, &qdata[queryLen+queryLen], sizeof(float) * queryLen);

                queryCaCords.x = query_x;
                queryCaCords.y = query_y;
                queryCaCords.z = query_z;
                vector<int> nnIntQuery;
                nnIntQuery = findNearestNeighbour(querynn, queryCaCords, querySeqLen, querySeq, qSeq);

                int passedNum = 0;
                int rejected = 0;
                while (*data != '\0' && passedNum < par.maxAccept && rejected < par.maxRejected) {
                    char dbKeyBuffer[255 + 1];
                    Util::parseKey(data, dbKeyBuffer);
                    data = Util::skipLine(data);
                    const unsigned int dbKey = (unsigned int) strtoul(dbKeyBuffer, NULL, 10);
                    unsigned int targetId = tdbr->getId(dbKey);
                    const bool isIdentity = (queryId == targetId && (par.includeIdentity || sameDB))? true : false;
                    if(isIdentity == true){
                        std::string backtrace = "";

                        int anat= (queryLen%4) ? (queryLen/4)*4+4 : queryLen;
                        for(int i=queryLen;i<anat;i++){
                            query_x[i]=0.0f;
                            query_x[i]=0.0f;
                            query_x[i]=0.0f;
                        }

                        Matcher::result_t result(dbKey, 0 , 1.0, 1.0, 1.0, 1.0, std::max(queryLen,queryLen), 0, queryLen-1, queryLen, 0, queryLen-1, queryLen, backtrace);
                        size_t len = Matcher::resultToBuffer(buffer, result, true, false);
                        resultBuffer.append(buffer, len);
                        continue;
                    }
                    char * targetSeq = tdbr->getData(targetId, thread_idx);
                    unsigned int targetSeqLen = tdbr->getSeqLen(targetId);

                    int targetLen = static_cast<int>(tdbr->getSeqLen(targetId));
                    float * tdata = (float*) tcadbr->getData(targetId, thread_idx);
                    tSeq.mapSequence(targetId, dbKey, targetSeq, targetSeqLen);

                    if(Util::canBeCovered(par.covThr, par.covMode, queryLen, targetLen)==false){
                        continue;
                    }
                    Coordinates targetCaCords;
                    memcpy(target_x, tdata, sizeof(float) * targetLen);
                    memcpy(target_y, &tdata[targetLen], sizeof(float) * targetLen);
                    memcpy(target_z, &tdata[targetLen+targetLen], sizeof(float) * targetLen);

                    targetCaCords.x = target_x;
                    targetCaCords.y = target_y;
                    targetCaCords.z = target_z;
                    vector<int> nnIntTarget = findNearestNeighbour(targetnn, targetCaCords, targetSeqLen, targetSeq, tSeq);
                    Matcher::result_t res = alignByNN(querynn, nnIntQuery, querySeq, queryLen, targetnn, nnIntTarget, targetSeq, targetSeqLen, &subMat, qSeq, tSeq, par.gapOpen.aminoacids, par.gapExtend.aminoacids, evaluer);
                    unsigned int targetKey = tdbr->getDbKey(targetId);
                    res.dbKey = targetKey;
                    //Matcher::result_t res = paruenAlign.align(qSeq, tSeq, &subMat, evaluer);
                    string cigar =  paruenAlign.backtrace2cigar(res.backtrace);

                    if (isIdentity) {
                        // set coverage and seqid of identity
                        res.qcov = 1.0f;
                        res.dbcov = 1.0f;
                        res.seqId = 1.0f;
                    }

                    //Add TM align score
                    Coordinates xtm(queryLen); Coordinates ytm(targetLen);
                    Coordinates r1(queryLen); Coordinates r2(targetLen);

                    // Matching residue index collection
                    paruenAlign.AlignedResidueIndex(res, ires);

                    // float t[3], u[3][3];
                    //double TMalnScore = get_score4pareun(r1, r2,  xtm, ytm, queryCaCords, targetCaCords, ires,
                    //                                     queryLen, t, u, mem);

                    //writing temp output file
                    //tempfile << queryId << "\t" << targetId << "\t" << res.score << "\t" << cigar << "\n";

                    if (Alignment::checkCriteria(res, isIdentity, par.evalThr, par.seqIdThr, par.alnLenThr, par.covMode, par.covThr)) {
                        alignmentResult.emplace_back(res);
                        passedNum++;
                        rejected = 0;
                    }

                    else {
                        rejected++;
                    }
                }
            }
            if (alignmentResult.size() > 1) {
                SORT_SERIAL(alignmentResult.begin(), alignmentResult.end(), Matcher::compareHits);
            }
            for (size_t result = 0; result < alignmentResult.size(); result++) {
                size_t len = Matcher::resultToBuffer(buffer, alignmentResult[result], par.addBacktrace);
                resultBuffer.append(buffer, len);
            }
            dbw.writeData(resultBuffer.c_str(), resultBuffer.length(), queryKey, thread_idx);
            resultBuffer.clear();
            alignmentResult.clear();
        }
        delete [] ires;
        free(querynn);
        free(targetnn);
    }

    dbw.close();
    resultReader.close();
    qdbr.close();
    qcadbr.close();
    if(sameDB == false){
        tdbr->close();
        tcadbr->close();
        delete tdbr;
        delete tcadbr;
    }
    return EXIT_SUCCESS;
}
