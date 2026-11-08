#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/GameStatsManager.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <chrono>

using namespace geode::prelude;

// ---------- Ajustes ----------
static int g_ticks = 0;
static bool on(char const* k) { return Mod::get()->getSavedValue<bool>(k, false); }
static void setOn(char const* k, bool v) { Mod::get()->setSavedValue<bool>(k, v); }

static const int FPS_LIST[] = {60, 120, 144, 240, 360};
static const float SPEED_LIST[] = {1.f, 0.25f, 0.5f, 0.75f, 1.5f, 2.f};
static int g_speedIdx = 0;

static int curFps() { return Mod::get()->getSavedValue<int>("fps", 60); }
static void applyFps() { CCDirector::get()->setAnimationInterval(1.0 / curFps()); }

// ---------- Funciones (hooks) ----------
class $modify(NCPlay, PlayLayer) {
    void destroyPlayer(PlayerObject* p, GameObject* o) {
        if (on("noclip") && o != m_anticheatSpike) return;
        PlayLayer::destroyPlayer(p, o);
    }
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        bool h = on("hitboxes");
        if (m_isDebugDrawEnabled != h) {
            m_isDebugDrawEnabled = h;
            this->updateDebugDraw();
        }
    }
    void resetLevel() {
        PlayLayer::resetLevel();
        if (on("practicesync") && m_isPracticeMode) {
            FMODAudioEngine::get()->setMusicTimeMS(
                static_cast<unsigned int>(m_gameState.m_levelTime * 1000.0), true, 0);
        }
    }
};

class $modify(TpsPlayer, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);
        auto gl = GJBaseGameLayer::get();
        if (gl && this == gl->m_player1) ++g_ticks;
    }
};

class $modify(ShakeGL, GJBaseGameLayer) {
    void shakeCamera(float d, float s, float i) {
        if (on("noshake")) return;
        GJBaseGameLayer::shakeCamera(d, s, i);
    }
};

class $modify(NoParticles, CCParticleSystemQuad) {
    void draw() {
        if (on("noparticles") && PlayLayer::get()) return;
        CCParticleSystemQuad::draw();
    }
};

class $modify(UnlockGM, GameManager) {
    bool isIconUnlocked(int id, IconType t) {
        return on("unlockall") || GameManager::isIconUnlocked(id, t);
    }
    bool isColorUnlocked(int id, UnlockType t) {
        return on("unlockall") || GameManager::isColorUnlocked(id, t);
    }
};

class $modify(UnlockGSM, GameStatsManager) {
    bool isItemUnlocked(UnlockType t, int id) {
        return on("unlockall") || GameStatsManager::isItemUnlocked(t, id);
    }
};

class $modify(VerifyEdit, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) return false;
        if (on("autoverify")) level->m_isVerified = true;
        return true;
    }
};

// ---------- Menú ----------
enum Kind { Header, Toggle, CycleFps, CycleSpeed };
struct Entry { Kind kind; char const* key; char const* label; };
static const Entry ENTRIES[] = {
    {Header, nullptr, "JUGADOR"},
    {Toggle, "noclip", "Noclip"},
    {Toggle, "hitboxes", "Hitboxes"},
    {Toggle, "practicesync", "Practice Music Sync"},
    {Header, nullptr, "DESBLOQUEO"},
    {Toggle, "unlockall", "Desbloquear iconos y colores"},
    {Toggle, "autoverify", "Auto-verificar mis niveles"},
    {Header, nullptr, "OPTIMIZACION"},
    {Toggle, "noshake", "Quitar shake"},
    {Toggle, "noparticles", "Quitar particulas"},
    {Toggle, "showtps", "Mostrar FPS / TPS"},
    {Header, nullptr, "BYPASS"},
    {CycleFps, nullptr, ""},
    {CycleSpeed, nullptr, ""},
};

class MenuPopup : public Popup {
    ButtonSprite* m_fpsSpr = nullptr;
    ButtonSprite* m_speedSpr = nullptr;
protected:
    bool init() {
        if (!Popup::init(310.f, 250.f)) return false;
        this->setTitle("Mod Menu");
        const float W = 270.f, H = 175.f, ROW = 32.f, HEAD = 24.f;
        auto size = m_mainLayer->getContentSize();

        float total = 6.f;
        for (auto& e : ENTRIES) total += (e.kind == Header ? HEAD : ROW);

        auto scroll = ScrollLayer::create({W, H});
        scroll->setPosition({size.width / 2 - W / 2, size.height / 2 - H / 2 - 12.f});
        m_mainLayer->addChild(scroll);
        auto content = scroll->m_contentLayer;
        content->setContentSize({W, std::max(total, H)});

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        content->addChild(menu);

        float y = std::max(total, H) - 3.f;
        int i = 0;
        for (auto& e : ENTRIES) {
            float h = (e.kind == Header ? HEAD : ROW);
            float cy = y - h / 2;
            if (e.kind == Header) {
                auto l = CCLabelBMFont::create(e.label, "goldFont.fnt");
                l->setScale(0.5f);
                l->setPosition({W / 2, cy});
                content->addChild(l);
            } else if (e.kind == Toggle) {
                auto t = CCMenuItemToggler::createWithStandardSprites(
                    this, menu_selector(MenuPopup::onToggle), 0.6f);
                t->setTag(i);
                t->toggle(on(e.key));
                t->setPosition({24.f, cy});
                menu->addChild(t);
                auto l = CCLabelBMFont::create(e.label, "bigFont.fnt");
                l->setAnchorPoint({0, 0.5f});
                l->limitLabelWidth(190.f, 0.45f, 0.2f);
                l->setPosition({46.f, cy});
                content->addChild(l);
            } else if (e.kind == CycleFps) {
                m_fpsSpr = ButtonSprite::create(fmt::format("Bypass FPS: {}", curFps()).c_str(),
                    200, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.55f);
                auto b = CCMenuItemSpriteExtra::create(m_fpsSpr, this, menu_selector(MenuPopup::onFps));
                b->setPosition({W / 2, cy});
                menu->addChild(b);
            } else {
                m_speedSpr = ButtonSprite::create(fmt::format("Velocidad: {}x", SPEED_LIST[g_speedIdx]).c_str(),
                    200, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.55f);
                auto b = CCMenuItemSpriteExtra::create(m_speedSpr, this, menu_selector(MenuPopup::onSpeed));
                b->setPosition({W / 2, cy});
                menu->addChild(b);
            }
            y -= h;
            ++i;
        }
        scroll->moveToTop();
        return true;
    }

    void onToggle(CCObject* s) {
        auto t = static_cast<CCMenuItemToggler*>(s);
        setOn(ENTRIES[t->getTag()].key, !t->isToggled());
    }
    void onFps(CCObject*) {
        int cur = curFps(), next = FPS_LIST[0];
        for (int k = 0; k < 5; ++k) if (FPS_LIST[k] == cur) next = FPS_LIST[(k + 1) % 5];
        Mod::get()->setSavedValue<int>("fps", next);
        applyFps();
        m_fpsSpr->setString(fmt::format("Bypass FPS: {}", next).c_str());
    }
    void onSpeed(CCObject*) {
        g_speedIdx = (g_speedIdx + 1) % 6;
        CCDirector::get()->getScheduler()->setTimeScale(SPEED_LIST[g_speedIdx]);
        m_speedSpr->setString(fmt::format("Velocidad: {}x", SPEED_LIST[g_speedIdx]).c_str());
    }
public:
    static MenuPopup* create() {
        auto ret = new MenuPopup();
        if (ret->init()) {
            ret->setID("modmenu-popup");
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------- Botón circular flotante ----------
class FloatingButton : public CCLayer {
    CCSprite* m_spr = nullptr;
    float m_base = 1.f;
    bool m_dragged = false;
    CCPoint m_start, m_offset;
    static constexpr float D = 40.f;
public:
    static FloatingButton* create() {
        auto ret = new FloatingButton();
        if (ret->init()) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
    bool init() override {
        if (!CCLayer::init()) return false;
        m_spr = CCSprite::create("logo.png"_spr);
        if (!m_spr) m_spr = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
        m_base = D / m_spr->getContentWidth();
        m_spr->setScale(m_base);
        this->addChild(m_spr);
        auto ws = CCDirector::get()->getWinSize();
        this->setPosition({Mod::get()->getSavedValue<float>("bx", ws.width - 40.f),
                           Mod::get()->getSavedValue<float>("by", ws.height / 2)});
        this->setTouchEnabled(true);
        return true;
    }
    void registerWithTouchDispatcher() override {
        CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -600, true);
    }
    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (!this->isVisible()) return false;
        if (this->convertToNodeSpace(t->getLocation()).getLength() > D / 2 + 4.f) return false;
        m_dragged = false;
        m_start = t->getLocation();
        m_offset = this->getPosition() - t->getLocation();
        m_spr->setScale(m_base * 0.9f);
        return true;
    }
    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        auto loc = t->getLocation();
        if (!m_dragged && loc.getDistance(m_start) > 8.f) m_dragged = true;
        if (m_dragged) {
            auto ws = CCDirector::get()->getWinSize();
            auto p = loc + m_offset;
            float r = D / 2;
            p.x = std::clamp(p.x, r, ws.width - r);
            p.y = std::clamp(p.y, r, ws.height - r);
            this->setPosition(p);
        }
    }
    void ccTouchEnded(CCTouch*, CCEvent*) override {
        m_spr->setScale(m_base);
        if (m_dragged) {
            Mod::get()->setSavedValue<float>("bx", this->getPositionX());
            Mod::get()->setSavedValue<float>("by", this->getPositionY());
            return;
        }
        auto scene = CCScene::get();
        if (scene && !scene->getChildByID("modmenu-popup")) {
            if (auto p = MenuPopup::create()) p->show();
        }
    }
    void ccTouchCancelled(CCTouch*, CCEvent*) override { m_spr->setScale(m_base); }
};

class Overlay : public CCNode {
    FloatingButton* m_btn = nullptr;
    CCLabelBMFont* m_info = nullptr;
    int m_frames = 0;
    std::chrono::steady_clock::time_point m_last = std::chrono::steady_clock::now();
public:
    static Overlay* create() {
        auto ret = new Overlay();
        if (ret->init()) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
    bool init() override {
        if (!CCNode::init()) return false;
        this->setZOrder(10000);
        m_btn = FloatingButton::create();
        this->addChild(m_btn);
        auto ws = CCDirector::get()->getWinSize();
        m_info = CCLabelBMFont::create("", "bigFont.fnt");
        m_info->setScale(0.35f);
        m_info->setAnchorPoint({0, 1});
        m_info->setPosition({4.f, ws.height - 4.f});
        m_info->setOpacity(180);
        this->addChild(m_info);
        this->scheduleUpdate();
        return true;
    }
    void update(float) override {
        // Se oculta dentro de niveles y editor
        m_btn->setVisible(!PlayLayer::get() && !LevelEditorLayer::get());
        ++m_frames;
        auto now = std::chrono::steady_clock::now();
        float el = std::chrono::duration<float>(now - m_last).count();
        if (el >= 1.f) {
            m_info->setVisible(on("showtps"));
            if (PlayLayer::get())
                m_info->setString(fmt::format("FPS {}  TPS {}", (int)(m_frames / el), (int)(g_ticks / el)).c_str());
            else
                m_info->setString(fmt::format("FPS {}", (int)(m_frames / el)).c_str());
            m_frames = 0; g_ticks = 0; m_last = now;
        }
    }
};

class $modify(StartMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        static bool done = false;
        if (!done) {
            done = true;
            applyFps();
            if (auto o = Overlay::create()) SceneManager::get()->keepAcrossScenes(o);
        }
        return true;
    }
};
