#include "productsControllers.h"
#include <models/Productcrud.h>
#include <drogon/orm/Mapper.h>
#include <json/json.h>

using namespace drogon;
using namespace drogon::orm;
using namespace std;

void productsControllers::createProducts(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback) {
    auto json = req->getJsonObject();
    Json::Value result;
    // check required field
    if (!json || !(*json).isMember("title") || !(*json).isMember("description") || !(*json).isMember("price") || !(*
            json).isMember("quantity")) {
        result["error"] = "Missing required fields";

        auto resp = HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }
    auto client = app().getDbClient();
    if (!client) {
        cout << "Database client not found!";
    }
    Mapper<drogon_model::shopapi::Productcrud> mapper(client);
    drogon_model::shopapi::Productcrud product;
    product.setTitle((*json)["title"].asString());
    product.setDescription((*json)["description"].asString());
    product.setPrice((*json)["price"].asDouble());
    product.setQuantity((*json)["quantity"].asInt());
    product.setImage((*json)["image"].asString());
    mapper.insert(product, [callback](const drogon_model::shopapi::Productcrud &p) {
                      Json::Value res;
                      res["status"] = "Success";
                      res["id"] = p.getValueOfId();
                      auto resp = HttpResponse::newHttpJsonResponse(res);
                      callback(resp);
                  },
                  [callback](const DrogonDbException &e) {
                      Json::Value res;
                      res["error"] = e.base().what();
                      auto resp = HttpResponse::newHttpJsonResponse(res);
                      resp->setStatusCode(k500InternalServerError);
                      callback(resp);
                  });
}
