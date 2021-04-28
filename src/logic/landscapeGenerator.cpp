#include <grend/geometryGeneration.hpp>
#include <math.h>
#include "landscapeGenerator.hpp"
#include <grend/gameEditor.hpp>

static float thing(float x, float y) {
	return sin(x) + sin(y);
}

static glm::vec2 randomGradient(glm::vec2 i) {
	/*
	float vx = 3141592623918.0*i.x + 31415926.0 + 1234558198*i.y;
	float vy = 314159261231.0*i.y + 3141592.0 + 9876598237*i.x;
	return { cos(vx), sin(vy) };
	*/
	float random = 2920.f
		* sin(i.x * 21942.f + i.y*171324.f + 8912.f)
		* cos(i.x * 23157.f * i.y*217832.f + 9758.f);
	return { cos(random), sin(random) };
}

static float dotGradient(glm::vec2 i, glm::vec2 pos) {
	glm::vec2 gradient = randomGradient(i);
	glm::vec2 dist = pos - i;

	return glm::dot(dist, gradient);
}

static float perlinNoise(float x, float y) {
	glm::vec2 pos = { x, y };
	glm::vec2 grid[2] = {
		glm::floor(pos),
		glm::floor(pos) + glm::vec2(1.0)
	};
	glm::vec2 weight = pos - grid[0];
	float n[4];

	for (unsigned i = 0; i < 4; i++) {
		n[i] = dotGradient(glm::vec2(grid[!!(i&1)].x, grid[!!(i&2)].y), pos);
	}

	float ix0 = glm::mix(n[0], n[1], weight.x);
	float ix1 = glm::mix(n[2], n[3], weight.x);

	return max(0.0, glm::mix(ix0, ix1, weight.y));
}

static float landscapeThing(float x, float y) {
	auto scalednoise = [](float scale, float x, float y) {
		return scale*perlinNoise(x / scale, y / scale);
	};

	float a1 = max(0.f, scalednoise(75.f, x, y));
	float a2 = scalednoise(20.f, x, y);
	float a3 = scalednoise(5.f,  x, y);
	float a4 = 0.5*scalednoise(1.f, x, y);

	return a1 + a2 + a3 + a4;
}

static const int   gridsize = 9;
static const float cellsize = 24.f;

landscapeGenerator::landscapeGenerator(unsigned seed) {
	// TODO: do something with the seed
}

landscapeGenerator::~landscapeGenerator() { }

void landscapeGenerator::generateLandscape(gameMain *game,
                                           glm::vec3 curpos,
                                           glm::vec3 lastpos)
{
	static gameModel::ptr models[gridsize][gridsize];
	static gameModel::ptr temp[gridsize][gridsize];
	static gameModel::ptr grassmod;
	// BIG XXX: avoid accessing landscape material shared pointer from multiple
	//          threads (assignment to mesh material increases use count)
	static std::mutex landscapemtx;

#define LOCAL_BUILD 0
#if LOCAL_BUILD
	if (grassmod == nullptr) {
		//grassmod = loadScene("./test-assets/obj/crapgrass.glb");
		//grassmod = loadScene("./test-assets/obj/smoothcube.glb");
		grassmod = load_object("assets-old/obj/Modular Terrain Hilly/Prop_Grass_Clump_2.obj");
		game->jobs->addDeferred([=] {
			compileModel("grassclump", grassmod);
			bindModel(grassmod);
			return true;
		});
	}

#else
	grassmod = std::make_shared<gameModel>();
#endif

	gameObject::ptr ret = std::make_shared<gameObject>();
	std::list<std::future<bool>> futures;

	glm::vec3 diff = curpos - lastpos;
	float off = cellsize * (gridsize / 2);
	SDL_Log("curpos != genpos, diff: (%g, %g, %g)", diff.x, diff.y, diff.z);

	for (int x = 0; x < gridsize; x++) {
		for (int y = 0; y < gridsize; y++) {
			int ax = x + diff.x;
			int ay = y + diff.z;

			if (models[x][y] == nullptr
			    || ax >= gridsize || ax < 0
			    || ay >= gridsize || ay < 0)
			{
				glm::vec3 coord =
					(curpos * cellsize)
					- glm::vec3(off, 0, off)
					+ glm::vec3(x*cellsize, 0, y*cellsize);

				glm::vec3 prev =
					(lastpos * cellsize)
					- glm::vec3(off, 0, off)
					+ glm::vec3(x*cellsize, 0, y*cellsize);

				if (models[x][y]) {
					// don't emit delete if there's no model there
					// (ie. on startup)
					emit((generatorEvent) {
							.type = generatorEvent::types::deleted,
							.position = prev + glm::vec3(cellsize*0.5, 0, cellsize*0.5),
							.extent = glm::vec3(cellsize * 0.5f, HUGE_VALF, cellsize*0.5f),
					});
				}

				emit((generatorEvent) {
					.type = generatorEvent::types::generatorStarted,
					.position = coord + glm::vec3(cellsize*0.5, 0, cellsize*0.5),
					.extent = glm::vec3(cellsize * 0.5f, HUGE_VALF, cellsize*0.5f),
				});

				SDL_Log("CCCCCCCC: generating coord (%g, %g)", coord.x, coord.z);

				// TODO: reaaaaallly need to split this up
				futures.push_back(game->jobs->addAsync([=] {
					SDL_Log("DDDDDDD: got here, from the future (%g, %g)",
							coord.x, coord.z);
					auto ptr = generateHeightmap(cellsize, cellsize, 2.0, coord.x, coord.z, landscapeThing);
					//auto ptr = generateHeightmap(24, 24, 0.5, coord.x, coord.z, thing);
					SDL_Log("EEEEEEE: generated model");
					TRS transform = ptr->getTransformTRS();
					transform.position = glm::vec3(coord.x, 0, coord.z);
					ptr->setTransform(transform);

					gameMesh::ptr mesh =
						std::dynamic_pointer_cast<gameMesh>(ptr->getNode("mesh"));
					gameMesh *fug = dynamic_cast<gameMesh*>(ptr->getNode("mesh").get());

					gameMesh *blarg = (gameMesh*)ptr->getNode("mesh").get();

					SDL_Log("EEEEEEE.v2: has node: %d, mesh ptr: %p, source ptr: %p, other? %p, id: %s, blarg: %p",
						ptr->hasNode("mesh"), mesh.get(),
						ptr->getNode("mesh").get(), fug, ptr->getNode("mesh")->idString().c_str(), blarg);

					if (mesh) {
						//std::lock_guard<std::mutex> g(landscapemtx);
						mesh->meshMaterial = std::make_shared<material>();
						mesh->meshMaterial->factors.diffuse = {0.15, 0.3, 0.1, 1};
						mesh->meshMaterial->factors.ambient = {1, 1, 1, 1};
						mesh->meshMaterial->factors.specular = {1, 1, 1, 1};
						mesh->meshMaterial->factors.emissive = {0, 0, 0, 0};
						mesh->meshMaterial->factors.roughness = 0.9f;
						mesh->meshMaterial->factors.metalness = 0.f;
						mesh->meshMaterial->factors.opacity = 1;
						mesh->meshMaterial->factors.refract_idx = 1.5;
					}
					//mesh->meshMaterial = landscapeMaterial;
					std::string name = "gen["+std::to_string(int(x))+"]["+std::to_string(int(y))+"]";
					SDL_Log("FFFFFFF: setting node");

					gameObject::ptr foo = std::make_shared<gameObject>();
					setNode("asdfasdf", foo, ptr);
					SDL_Log("GGGGGGG: Adding landscape mesh to physics");

					auto fut = game->jobs->addDeferred([=]{
						SDL_Log("HHHHHHH: Generating new landscape model");
						compileModel(name, ptr);
						bindModel(ptr);
						return true;
					});

					SDL_Log("IIIIIII: generating tree instances");
					glm::vec2 posgrad = randomGradient(glm::vec2(coord.x, coord.z));
					float baseElevation = landscapeThing(coord.x, coord.z);
					int randtrees = (posgrad.x + 1.0)*0.5 * 5 * (1.0 - baseElevation/50.0);

					game->phys->addStaticModels(nullptr, foo, TRS());

					gameParticles::ptr parts = std::make_shared<gameParticles>(32);
					parts->activeInstances = randtrees;
					parts->radius = cellsize / 2.f * 1.415;

					for (unsigned i = 0; i < parts->activeInstances; i++) {
						TRS transform;
						glm::vec2 pos = randomGradient(glm::vec2(coord.x + i, coord.z + i));

						float tx = ((pos.x + 1)*0.5) * cellsize;
						float ty = ((pos.y + 1)*0.5) * cellsize;

						transform.position = glm::vec3(
							tx, landscapeThing(coord.x + tx, coord.z + ty) - 0.1, ty
						);
						transform.scale = glm::vec3((posgrad.y + 1.0)*0.5*3.0+0.5);
						parts->positions[i] = transform.getTransform();
					}

					SDL_Log("JJJJJJJ: adding node instances...");
					parts->update();
					setNodeXXX("tree", parts, treeNode);
					setNodeXXX("parts", ptr, parts);
					SDL_Log("KKKKKKK: ok cool");

#if 0
					int randgrass = (posgrad.y*0.5 + 0.5) * 256 * (1.0 - baseElevation/50.0);
					gameParticles::ptr grass = std::make_shared<gameParticles>(256);
					grass->activeInstances = randgrass;
					grass->radius = cellsize * 1.415;

					for (unsigned i = 0; i < grass->activeInstances; i++) {
						TRS transform;
						/*
						glm::vec2 pos = randomGradient(glm::vec2(coord.x + i, coord.z + i));

						float tx = ((pos.x + 1)*0.5) * cellsize;
						float ty = ((pos.y + 1)*0.5) * cellsize;
						*/
						auto fract = [](float n){ return n - floor(n); };
						glm::vec2 pos(fract(sin(1234567.89*(coord.x + i))), fract(sin(123456789.10*(coord.y + i))));

						float tx = pos.x * cellsize;
						float ty = pos.y * cellsize;

						transform.position = glm::vec3(
							tx, landscapeThing(coord.x + tx, coord.z + ty), ty
						);
						//transform.scale = glm::vec3((posgrad.y + 1.0)*0.5*3.0+0.5);
						grass->positions[i] = transform.getTransform();
					}

					grass->update();
					setNodeXXX("grass", grass, grassmod);
					setNodeXXX("grassparts", ptr, grass);

					int randlight = (posgrad.y + 1.0)*0.5 * 7 * (1.0 - baseElevation/50.0);

					for (int i = 0; i < randlight; i++) {
						gameLightPoint::ptr nlit = std::make_shared<gameLightPoint>();
						glm::vec2 pos = randomGradient(glm::vec2(2*coord.x + i, 2*coord.z + i));

						float tx = ((pos.x + 1)*0.5) * cellsize;
						float ty = ((pos.y + 1)*0.5) * cellsize;

						glm::vec3 colors[6] = {
							{1.0, 0.5, 0.2},
							{1.0, 0.2, 0.5},
							{0.5, 1.0, 0.2},
							{0.5, 0.2, 1.0},
							{0.2, 1.0, 0.5},
							{0.2, 0.5, 1.0},
						};

						nlit->radius = 0.30;
						nlit->intensity = 500.0;
						nlit->diffuse = glm::vec4(colors[rand() % 6], 1.0);
						nlit->transform.position = glm::vec3(
							tx, landscapeThing(coord.x + tx, coord.z + ty) + 1.5, ty
						);

						std::string name = "point["+std::to_string(i)+"]";
						setNodeXXX(name, ptr, nlit);
					}
#endif

					temp[x][y] = ptr;
					fut.wait();
					return true;
				}));

				emit((generatorEvent) {
					.type = generatorEvent::types::generated,
					.position = coord + glm::vec3(cellsize*0.5, 0, cellsize*0.5),
					.extent = glm::vec3(cellsize*0.5f, HUGE_VALF, cellsize*0.5f),
				});

			} else {
				temp[x][y] = models[ax][ay];
			}
		}
	}

	for (auto& fut : futures) {
		fut.wait();
	}

	auto meh = game->jobs->addDeferred([&] {
		for (int x = 0; x < gridsize; x++) {
			for (int y = 0; y < gridsize; y++) {
				models[x][y] = temp[x][y];
				temp[x][y] = nullptr;
				std::string name = "gen["+std::to_string(int(x))+"]["+std::to_string(int(y))+"]";
				setNode(name, ret, models[x][y]);
			}
		}

		return true;
	});
	meh.wait();

	/*
	auto fut = game->jobs->addDeferred([&] {
		//bindCookedMeshes();
		return true;
	});

	fut.wait();
	*/
	returnValue = ret;
}

void landscapeGenerator::setPosition(gameMain *game, glm::vec3 position) {
	glm::vec3 curpos = glm::floor((glm::vec3(1, 0, 1)*position)/cellsize);

	if (genjob.valid() && genjob.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
		genjob.get();
		setNode("nodes", root, returnValue);
		returnValue = nullptr;
	}

	if (!genjob.valid() && curpos != lastPosition) {
		SDL_Log("BBBBBBBBBB: starting new generator job");
		glm::vec3 npos = lastPosition;
		lastPosition = curpos;

		genjob = game->jobs->addAsync([=] {
			generateLandscape(game, curpos, npos);
			return true;
		});
	}
}
