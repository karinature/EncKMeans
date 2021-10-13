

#ifndef ENCKMEAN_DATASERVER_H
#define ENCKMEAN_DATASERVER_H


#include "Client.h"

class DataServer {

    //    [[maybe_unused]] const helib::SecKey encryptionKey;
    //    [[maybe_unused]] const helib::PubKey encryptionKey;
    //    [[maybe_unused]] const helib::EncryptedArray ea;
    const helib::PubKey encryptionKey;
    //     const helib::EncryptedArray ea;
    const helib::Ctxt scratch;
protected:
    //    const helib::PubKey &public_key;// = encryptionKey;
    KeysServer &keysServer;
public:
    /**
     * Constructor for \class{Client},
     * @param keysServer binds to the \class{KeysServer} responsible for the distributing the appropriate key
     * @brief simulates a mini-protocol between the keys server and the Data Server.
     * ks and ds will create a scratch that will allow to encrypt the results:
     * result bit in case of compare, and result num in case of add/multiplication.
     * */
    explicit DataServer(KeysServer &keysServer);

    Point scratchPoint();

    /*  Generate random data.
     *  Returns a vector of clients with random points.
    *      client [0] stays empty
    *      client [1] has 1 point - {point0}
    *      client [2] has 2 points - {point0, point1}
    *      ...
    *      client [n] has n points -  {point0, point1, ... , pointN}
    */  // todo move to aux ?
    static std::vector<Client> generateDataClients(const KeysServer &server) {
        int uniquePointsNum = 3 + rand() % number_of_points,
                clientsNum = uniquePointsNum;
        /*        //  init coordinate arrays
                //        long arrs[uniquePointsNum][DIM];
                //        for (auto &arr: arrs)
                //            for (short dim = 0; dim < DIM; ++dim)
                //                arr[dim] = rand() % NUMBERS_RANGE;*/
        long arr[DIM];
        std::vector<Client> clients(clientsNum, Client(server));
        for (int i = 1; i < clients.size(); ++i)
            for (int j = 0; j < i; ++j) {
                //  pick random coordinates
                for (short dim = 0; dim < DIM; ++dim)
                    arr[dim] = rand() % NUMBERS_RANGE;
                //  encrypt coordinates and add to client
                clients[i].encryptPoint(arr);
                //                clients[i].encryptPoint(arrs[j]);}
            }
        return clients;
    }


    /**
     * @brief A simulated retrievel of data from clients.
     * @param clients - a list of clients (chosen by the CA, to share a similar public key).
     * @returns a list of all the points.
* @return std::vector<Point>
     * * */
    static std::vector<Point> retrievePoints(std::vector<Client> &clients) {
        std::vector<Point> points;
        if (clients.empty()) return points;
        points.reserve(pow(clients.size(), 2) / 2); // preallocate memory
        for (Client &c:clients)
            for (Point &p:c.points)
                points.emplace_back(Point(p));
        //            points.insert(points.end(),c.points.begin(), c.points.end());
        points.shrink_to_fit();
        return points;
    }

    /**
     * @brief Picks a group of random points to be used as both cell representative and max-limit.
     * Uses the Fisher–Yates shuffle for choosing k random points.
     * @parampoints - a list of all points (in current group).
     * @param numOfReps - the desired number of representatives, usually the number of desired data strips.
     * @returns a sub list of the original points, picked randomly using the .
     * @return std::vector<Point>
     * */
    static std::vector<Point>
    pickRandomPoints(std::vector<Point> &points, int numOfStrips = int(1 / epsilon)) {
        // sanity check
        if (points.empty() || numOfStrips > points.size()) return std::vector<Point>();

        std::vector<Point> copy(points);
        auto begin = copy.begin();
        int k = numOfStrips, stripSize = numOfStrips;
        while (k--) {
            auto r = begin;
            // next line used to crushes the program with small(<20) number of points in file
            advance(r, random() % stripSize);
            swap(begin, r);
            ++begin;
            --stripSize;
        }
        //  todo see if there's a way to reduce this copy
        //        std::vector<Point> random(copy.begin(), copy.begin() + numOfStrips - 1);
        return std::vector<Point>(copy.begin(), copy.begin() + numOfStrips - 1);
    }

    /**
     * @brief Split into (1/eps) groups - each group is between 2 representative points.
     * @param points - a list of unordered points
     * @param dim - the index of coor by which the comparison is made. in other words - in what dimension the split is made.
     * @param keysServer - the appointed CA
     * @returns a list of pairs/tuples of a representative point and a list of the points in its Group (cell/strip).
     * */
    static std::vector<
            std::tuple< //should be replaced with std::pair in production
                    Point,
                    std::vector<Point>,
                    std::vector<Ctxt>
            >
    >
    split(std::vector<Point> &points, short dim, KeysServer &keysServer) {
        cout << "Split" << endl;
        auto t0_split = std::chrono::high_resolution_clock::now();
        std::vector<std::tuple<Point, std::vector<Point>, std::vector<Ctxt> > > groups;
        if (points.empty() || 0 > dim || DIM < dim) return groups;        // sanity check
        //        if (points.empty() || 0 > dim || DIM < dim) return std::vector<std::tuple<Point, std::vector<Point>, std::vector<Ctxt> > >();        // sanity check

        const std::vector<Point> randomPoints = DataServer::pickRandomPoints(points);
        //  create points compairing dict - for every 2 points (p1,p2) answers p1[dim]>p2[dim]
        std::map<const Point,
                std::map<const Point,
                        helib::Ctxt, cmpPoints>, cmpPoints>
                cmp = createCmpDict(randomPoints, points, 0);

        for (const Point &R: randomPoints) {
            auto t0_itr = std::chrono::high_resolution_clock::now();
            cout << "\n\n   ### current R: ";
            printPoint(R, keysServer);

            std::vector<Point> group;
            group.reserve(points.size() * 2 * epsilon);
            std::vector<Ctxt> groupSize;
            groupSize.reserve(points.size() * 2 * epsilon);

            for (const Point &p:points) {
                cout << "\n       === current p: ";
                printPoint(p, keysServer);

                // p < R
                helib::Ctxt isBelowCurrentRep(cmp[R].at(p));
                printNameVal(keysServer.decryptCtxt(isBelowCurrentRep));
                helib::Ctxt isAboveAboveSmallerReps(isBelowCurrentRep); //todo other init
//                printNameVal(keysServer.decryptCtxt(isAboveAboveSmallerReps));

                for (const Point &r: randomPoints) {
                    cout << "           --- other r: ";
                    printPoint(r, keysServer);
                    if (r.id == R.id ) continue;
                    //todo here's a good place to check for "tail" p - is curr p bigger than all the random points
                    //todo handle cases of p that is equal to representative

                    //  r > R (in which case we don't care about cmp results of p and r)
                    Bit otherRepIsAboveCurrentRep = cmp[r].at(R);
                    // results in: Bit otherRepIsAboveCurrentRep = (r > R)
                    printNameVal(keysServer.decryptCtxt(otherRepIsAboveCurrentRep));

                    //  R > r    AND     p > r
                    Bit pIsAboveOtherRep(cmp[R].at(r)); //pIsAboveOtherRep = (R > r)
                    pIsAboveOtherRep.multiplyBy(cmp[p].at(r));
                    // results in: Bit pIsAboveOtherRep = (R > r) * (p > r)
                    printNameVal(keysServer.decryptCtxt(pIsAboveOtherRep));

                    //   R < r  OR  [ R > r    AND     p > r ]
                    Bit pIsBelowCurrentRepAndAboveOtherRep = pIsAboveOtherRep;
                    pIsBelowCurrentRepAndAboveOtherRep.multiplyBy(otherRepIsAboveCurrentRep);
                    pIsBelowCurrentRepAndAboveOtherRep.negate();
                    pIsBelowCurrentRepAndAboveOtherRep += pIsAboveOtherRep;
                    pIsBelowCurrentRepAndAboveOtherRep += otherRepIsAboveCurrentRep;
                    // results in: Bit pIsBelowCurrentRepAndAboveOtherRep =
                    // pIsAboveOtherRep + otherRepIsAboveCurrentRep
                    // - pIsAboveOtherRep * otherRepIsAboveCurrentRep
                    printNameVal(keysServer.decryptCtxt(pIsBelowCurrentRepAndAboveOtherRep));

                    // this will hold the Product[ (R > r) && (p > r) | foreach r in randomPoints ]
                    isAboveAboveSmallerReps *= pIsBelowCurrentRepAndAboveOtherRep;
                    printNameVal(keysServer.decryptCtxt(isAboveAboveSmallerReps));
                }
                Bit isInCell = isBelowCurrentRep;
                isInCell *= isAboveAboveSmallerReps;
                printNameVal(keysServer.decryptCtxt(isInCell));

                Point pointIsInCell = p * isInCell;
                cout << "\nPoint is in cell?";
                printPoint(pointIsInCell, keysServer);
                cout << "\n";


                group.push_back(pointIsInCell);
                groupSize.emplace_back(isInCell);
            }

            // fixme need to implement hash function for Point to use map ?
            //      `in instantiation of member function 'std::less<Point>::operator()' requested here`
            groups.emplace_back(R, group, groupSize);
            printDuration(t0_itr, "split iteration");
        }

        // todo "tail" points - all the points that are bigger than all random reps

        //
        //        for (std::tuple tuple: groups) {
        //            cout << "curtuple" << endl;
        //            Point randomPoint = std::get<0>(tuple);
        //            std::vector<Point> group = std::get<1>(tuple);
        //            std::vector<Ctxt> groupSize = std::get<2>(tuple);
        //            cout << "for random point: ";
        //            printPoint(randomPoint, keysServer);
        //            cout << " these points will be included: \n";
        //            printPoints(group, keysServer);
        //            cout << "group size: " << keysServer.decryptSize(groupSize) << endl;
        //            //        for (Point &point:group) {
        //            //        }
        //        }

        printDuration(t0_split, "split ");
        return groups;
    }


    static std::map<const Point,
            std::map<const Point,
                    //                    std::vector<Bit>,
                    helib::Ctxt,
                    cmpPoints
            >,
            cmpPoints
    >
    createCmpDict(const std::vector<Point> &randomPoints,
                  const std::vector<Point> &allPoints,
                  short dim) {
        std::map<const Point, std::map<const Point, helib::Ctxt, cmpPoints>, cmpPoints> cmpDict;
        for (const Point &point : randomPoints) {
            //            std::map<Point, std::vector<Bit>, cmpPoints> cmpDictMini; //, cmpDictMini2;
            for (const Point &point2 : allPoints) {
                //                if (!cmpDict[point].empty() && !cmpDict[point][point2].isEmpty()))
                //                if (point.id == point2.id) continue; //this is checked inside isBigger function
                // need to check if exist for efficiency
                std::vector<helib::Ctxt> res = point.isBiggerThan(point2, dim);
                cmpDict[point].emplace(point2, res[0]);   //  point > point2
                cmpDict[point2].emplace(point, res[1]);   //  point < point2
            }
        }
        return cmpDict;
    }

};


#endif //ENCKMEAN_DATASERVER_H
