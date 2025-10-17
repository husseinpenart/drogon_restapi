#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class productsControllers : public drogon::HttpController<productsControllers> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(productsControllers::createProducts, "/api/products", Post);
        // ADD_METHOD_TO(productsControllers::getAllProducts, "/api/getProducts", Get);
        // ADD_METHOD_TO(productsControllers::getProductById, "/api/product/{1}", Get);
        // ADD_METHOD_TO(productsControllers::updateProducts, "/api/updateProducts/{1}", Put);
        // ADD_METHOD_TO(productsControllers::deleteProduct, "/api/deleteProduct/{1}", Delete);
    METHOD_LIST_END

    // functions to set repo
    static void createProducts(const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);
    //
    // void getAllProducts(const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);
    //
    // void getProductById(const HttpRequestPtr &ptr, std::function<void (const HttpResponsePtr &)> &&callback, int id);
    //
    // void updateProducts(const HttpRequestPtr &ptr, std::function<void (const HttpResponsePtr &)> &&callback, int id);
    //
    // void deleteProduct(const HttpRequestPtr &ptr, std::function<void (const HttpResponsePtr &)> &&callback);
};
