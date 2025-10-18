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
    std::cout << "Loaded DB clients: " << drogon::app().getDbClient("default") << std::endl;

    auto client = app().getDbClient();

    if (!client) {
        cout << "Database client not found!";
    }
    cout << "Im withhout db: " << endl;

    Mapper<drogon_model::shopapi::Productcrud> mapper(client);
    drogon_model::shopapi::Productcrud product;
    product.setTitle((*json)["title"].asString());
    product.setDescription((*json)["description"].asString());
    product.setPrice((*json)["price"].asDouble());
    product.setQuantity((*json)["quantity"].asInt());
    product.setImage((*json)["image"].asString());
    mapper.insert(product, [callback](const drogon_model::shopapi::Productcrud &p) {
                      Json::Value res;
                      res["message"] = "Product inserted successfully!";
                      res["status"] = "success";
                      // Return all fields of the inserted record
                      Json::Value data;
                      data["id"] = p.getValueOfId();
                      data["title"] = p.getValueOfTitle();
                      data["description"] = p.getValueOfDescription();
                      data["price"] = p.getValueOfPrice();
                      data["quantity"] = p.getValueOfQuantity();
                      data["image"] = p.getValueOfImage();
                      res["data"] = data;
                      auto resp = HttpResponse::newHttpJsonResponse(res);
                      resp->setStatusCode(k200OK);
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


void productsControllers::getAllProducts(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback) {
    Json::Value result;

    try {
        // connecting to database
        orm::Mapper<drogon_model::shopapi::Productcrud> mapper(app().getDbClient());
        // 2️⃣ Get all products from DB
        auto products = mapper.findAll();
        // 3️⃣ Convert results to JSON array
        Json::Value data(Json::arrayValue);
        for (auto &p: products) {
            data.append(p.toJson());
        }
        // 4️⃣ Create success JSON response
        result["message"] = "list of products fetched successfully";
        result["status"] = "success";
        result["data"] = data;
        auto resp = HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const std::exception &e) {
        // 5️⃣ Handle DB or other errors
        result["status"] = "failed";
        result["message"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}
void productsControllers::getProductById(const HttpRequestPtr &ptr, std::function<void(const HttpResponsePtr &)> &&callback, int id) {
    Json::Value res;

    try {
     orm::Mapper<drogon_model::shopapi::Productcrud>mapper(app().getDbClient());
        auto product = mapper.findByPrimaryKey(id);
        res["status"] = "success";
        res["data"] = product.toJson();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const drogon::orm::UnexpectedRows & e) {
        // Handle "not found" case
        res["status"] = "failed";
        res["message"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k404NotFound);
        callback(resp);
    } catch (const std::exception &e) {
        res["status"] = "failed";
        res["message"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}
