// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
void TestAddDocument() {
    SearchServer server;
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector <int> rating = {1, 2, 3};

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, rating);
        ASSERT_EQUAL(server.GetDocumentCount(), 1u);
        vector <Document> testvec = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(testvec.size(), 1u);
}

// Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
void TestForMinusWordsExludeFromSearch() {
    SearchServer server;
    const vector <int> ratings = {1, 2, 3};
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, "dog in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(3, "parrot in the city"s, DocumentStatus::ACTUAL, ratings);
    {
        const auto found_docs = server.FindTopDocuments("city -cat"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
    }
    {
        ASSERT_HINT(server.FindTopDocuments("in -city"s).empty(),
        "Documents containing Minus Words should not be included in search results."s);
    }

}

// Матчинг документов
void TestMatchingWords(){
    SearchServer server;
    const vector <int> ratings = {1, 2, 3};
    DocumentStatus status = DocumentStatus::ACTUAL;
    vector<string> match_docs;
    server.AddDocument(1, "cat in the city"s, status, ratings);
    server.AddDocument(2, "dog in the city"s, status, ratings);
    
    ASSERT_EQUAL(server.GetDocumentCount(), 2u);
    
    {
        tie(match_docs, status) = server.MatchDocument("cat in the city"s, 1);
        ASSERT_EQUAL(match_docs.size(), 4);
    }
    {
        tie(match_docs, status) = server.MatchDocument("cat in the city"s, 2);
        ASSERT_EQUAL(match_docs.size(), 3);
    }

}

// Сортировка найденных документов по релевантности.
void TestForSort() {
    SearchServer server;
        server.SetStopWords("и в на"s);

        server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

        const auto result = server.FindTopDocuments("пушистый ухоженный кот"s);
        const auto result_0 = result[0].id;
        const auto result_1 = result[1].id;
        const auto result_2 = result[2].id;

        ASSERT_EQUAL(result[0].id, result_0);
        ASSERT_EQUAL(result[1].id, result_1);
        ASSERT_EQUAL(result[2].id, result_2);

}

// Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestRatingDocument() {
    SearchServer server;
    {
        const vector<int> rating = {8, -3};
        int sum_ratings = 0;
        for (const int irating : rating) {
             sum_ratings += irating;
             }

        int avg_rating = sum_ratings / static_cast<int>(rating.size());

        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, rating);
        vector<Document> testvec = server.FindTopDocuments("белый кот и модный ошейник"s);
        ASSERT_EQUAL(avg_rating, testvec[0].rating);
   }
   {
       const vector<int> rating = {7, 2, 7};
       int sum_ratings = 0;
       for (const int irating : rating) {
            sum_ratings += irating;
            }

       int avg_rating = sum_ratings / static_cast<int>(rating.size());

       server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, rating);
       vector<Document> testvec = server.FindTopDocuments("пушистый кот пушистый хвост"s);
       ASSERT_EQUAL(avg_rating, testvec[0].rating);
   }
}

// Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestForPredicate() {
    SearchServer server;
    server.SetStopWords("и в на"s);

    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});


        ASSERT_EQUAL(server.FindTopDocuments("кот"s).size(), 2u);
        ASSERT_EQUAL(server.FindTopDocuments("ухоженный"s,DocumentStatus::ACTUAL).size(), 1u);
        ASSERT_EQUAL(server.FindTopDocuments("ухоженный"s,DocumentStatus::BANNED).size(), 1u);

}

// Поиск документов, имеющих заданный статус.
void TestForDocumentsStatus() {
    SearchServer server;
    server.SetStopWords("и в на"s);

    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    {
    auto testvec = server.FindTopDocuments("кот"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(testvec.size(), 2u);
    }

    {
    auto testvec = server.FindTopDocuments("ухоженный"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(testvec.size(), 1u);
    }
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestForMinusWordsExludeFromSearch);
    RUN_TEST(TestMatchingWords);
    RUN_TEST(TestForSort);
    RUN_TEST(TestRatingDocument);
    RUN_TEST(TestForPredicate);
    RUN_TEST(TestForDocumentsStatus);
}

// --------- Окончание модульных тестов поисковой системы -----------
