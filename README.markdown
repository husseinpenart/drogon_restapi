# Drogon REST API

This is a REST API built with the **Drogon C++ framework**, designed for managing products with full CRUD operations, user authentication/authorization using JWT, and image upload functionality. It uses **PostgreSQL** as the database and is containerized with **Docker** for easy deployment.

The project leverages modern C++ (C++17) and libraries like `jwt-cpp`, `fmt`, and `uuid` to handle authentication, formatting, and unique IDs. It’s a lightweight, high-performance backend suitable for small to medium-scale applications.

## Features

- **Product CRUD**: Create, read, update, and delete products.
- **User Authentication/Authorization**: Secure endpoints with JWT-based authentication.
- **Image Upload**: Upload and store product images.
- **PostgreSQL Integration**: Store data in a relational database.
- **Dockerized**: Run the API in a container for portability and consistency.

## Tech Stack

- **Framework**: Drogon (C++ web framework)
- **Database**: PostgreSQL
- **Libraries**:
  - `jwt-cpp`: JSON Web Token handling
  - `fmt`: String formatting
  - `uuid`: Unique ID generation
  - C++17 standard library (`filesystem`, `algorithm`, etc.)
- **Containerization**: Docker
- **Build Tool**: CMake

## Prerequisites

- **Docker**: Install [Docker Desktop](https://www.docker.com/products/docker-desktop/) (Windows/Mac) or Docker (Linux).
- **PostgreSQL**: A running PostgreSQL instance (local or Dockerized).
- **CMake**: For local development (optional if using Docker).
- **Git**: To clone the repository.

## Getting Started

### Clone the Repository

```bash
git clone https://github.com/husseinpenart/drogon_restapi.git
cd drogon_restapi
```

### Database Setup

1. Start a PostgreSQL container:

```bash
docker run -d --name postgres -e POSTGRES_USER=user -e POSTGRES_PASSWORD=pass -e POSTGRES_DB=mydb -p 5432:5432 postgres:14
```

2. Update `config.json` in the project root with your database credentials:

```json
{
  "db_client": {
    "name": "default",
    "rdbms": "postgresql",
    "host": "postgres",
    "port": 5432,
    "dbname": "mydb",
    "user": "user",
    "passwd": "pass"
  }
}
```

3. Initialize the database schema (if your app uses Drogon’s ORM):
   - Run migrations or SQL scripts as defined in your `models/` directory.

### Build and Run with Docker

1. Build the Docker image:

```bash
docker build -t drogon_restapi .
```

2. Create a Docker network for the API and database:

```bash
docker network create my-network
docker network connect my-network postgres
```

3. Run the API container:

```bash
docker run -d --name drogon_restapi --network my-network -p 8080:8080 drogon_restapi
```

4. Access the API at `http://localhost:8080`.

### Local Development (Optional)

If you prefer to run without Docker:

1. Install dependencies:
   - Ubuntu:

```bash
sudo apt-get install g++ cmake make libjsoncpp-dev zlib1g-dev libssl-dev libbrotli-dev uuid-dev libpq-dev libfmt-dev
```

   - Windows: Use vcpkg or another package manager to install Drogon, JWT-CPP, fmt, and PostgreSQL client libraries.

2. Build with CMake:

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

3. Run the binary:

```bash
./drogon_restapi
```

## API Endpoints

- **POST /auth/login**: Authenticate a user and return a JWT.
- **POST /auth/register**: Register a new user.
- **GET /products**: List all products (authenticated).
- **POST /products**: Create a new product with optional image upload (authenticated).
- **GET /products/{id}**: Get a product by ID.
- **PUT /products/{id}**: Update a product (authenticated).
- **DELETE /products/{id}**: Delete a product (authenticated).

*Note*: Replace `{id}` with the actual product ID. Check your `controllers/` directory for exact endpoint definitions.

## Project Structure

```
drogon_restapi/
├── config.json          # Drogon configuration (DB settings, etc.)
├── controllers/        # API endpoint logic (productsControllers.h, userControllers.h)
├── models/             # Database models (Productcrud.h, Usercase.h)
├── CMakeLists.txt      # CMake build configuration
├── Dockerfile          # Docker build instructions
├── .dockerignore       # Files to exclude from Docker context
└── main.cc             # Entry point
```

## Troubleshooting

- **Docker build fails**: Check network connectivity:

```bash
docker run --rm ubuntu:22.04 apt-get update
```

  Try a different mirror in the Dockerfile (e.g., `us.archive.ubuntu.com`).

- **DB connection issues**: Verify `config.json` and ensure the PostgreSQL container is running:

```bash
docker exec -it postgres psql -U user -d mydb
```

- **CMake errors**: Ensure all dependencies (Drogon, JWT-CPP, fmt) are installed or vendored. Share your `CMakeLists.txt` for help.

- **Windows line endings**: Use LF line endings in Git:

```bash
git config --global core.autocrlf false
```

## Contributing

Feel free to open issues or submit pull requests on [GitHub](https://github.com/husseinpenart/drogon_restapi). Feedback and improvements are welcome!

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.