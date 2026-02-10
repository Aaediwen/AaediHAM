/*
class map_overlay {
    private:
        struct transparancy {
            SDL_Texture* texture;
            enum mod_name owner;
        };
        std::vector<struct transparancy> overlay_list;
    public:
        map_overlay();
        ~map_overlay();
        get_overlay(SDL_Renderer renderer, enum mod_name owner); // return existing if present, or create a new and return that
        remove_overlay(enum mod_name owner); // remove any overlay owned by owner
        SDL_Texture* next_overlay();   // somehow get or use a read-only itterator through overlay_list
        clear(); // nuke all overlays
}

*/

void map_overlay::map_overlay () {
    index = 0;
    return;
}

void map_overlay::~map_overlay() {
    overlay_list.clear();
    return;
}

void map_overlay::clear() {
    overlay_list.clear();
    index = 0;
    return;
}
SDL_Texture* map_overlay::get_overlay(SDL_Renderer* renderer, enum mod_name owner, SDL_FRect dims) {

    for (auto overlay : overlay_list) {
        if (overlay.owner == owner) {
            return overlay.texture;
        }
    }
    if (renderer) {
        struct transparancy new_overlay;
        new_overlay.owner = owner;
        new_overlay.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, dims.w, dims.h);
        overlay_list.push_back(new_overlay);
        return (new_overlay.texture);
    }
    return nullptr;

}

void map_overlay::remove_overlay(enum mod_name owner) {
    for (int x = 0 ; x < overlay_list.size() ; x++ ) {
        if (overlay_list[i].owner == owner) {
            overlay_list.erase(i);
            return;
        }
    }
    return;
}

