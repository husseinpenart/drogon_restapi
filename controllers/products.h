#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class products : public drogon::HttpController<products> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(products::createProducts, "/api/products", Post);
        ADD_METHOD_TO(products::getAllProducts, "/api/getProducts", Get);
        ADD_METHOD_TO(products::getProductById, "/api/product/{1}", Get);
        ADD_METHOD_TO(products::updateProducts, "/api/updateProducts", Put);
        ADD_METHOD_TO(products::deleteProduct, "/api/deleteProduct", Delete);

    METHOD_LIST_END

    // functions to set repo
    void createProducts(const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);

    void getAllProducts(const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);

    void getProductById(const HttpRequestPtr &ptr, std::function<void (const HttpResponsePtr &)> &&callback, int id);

    void updateProducts(const HttpRequestPtr &ptr, std::function<void (const HttpResponsePtr &)> &&callback, int id);

    void deleteProduct(const HttpRequestPtr &ptr, std::function<void (const HttpResponsePtr &)> &&callback);
};
