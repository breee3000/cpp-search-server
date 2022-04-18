#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    vector<string> data;
    set<int> ids_to_remove;

    for (const int document_id : search_server) {
        std::map<std::string, double> x;
        string s;
        x = search_server.GetWordFrequencies(document_id);
        for (auto it = x.begin(); it != x.end(); ++it) {
            s += it->first;
        }
        data.push_back(s);
    }

    for (int i = 0; i < data.size(); ++i) {
        for (int j = 1; j < data.size(); ++j) {
            if (data[i] == data[j] && i < j) {
                ids_to_remove.insert(j + 1);
            }
        }
    }

    for (auto id: ids_to_remove) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}
