// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0

#include "SSVOpenHexagon/Core/HexagonGame.hpp"
#include "SSVOpenHexagon/Components/CWall.hpp"
#include "SSVOpenHexagon/Components/CCustomWall.hpp"
#include "SSVOpenHexagon/Utils/Color.hpp"
#include "SSVOpenHexagon/Utils/Ticker.hpp"

#include "SSVOpenHexagon/Global/Config.hpp"

#include <SSVStart/Utils/SFML.hpp>
#include <SSVStart/Utils/Vector2.hpp>

#include <SSVUtils/Core/Common/Frametime.hpp>
#include <SSVUtils/Core/Utils/Math.hpp>

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>

namespace hg
{

inline constexpr float baseThickness{5.f};

CPlayer::CPlayer(const sf::Vector2f& mPos) noexcept
    : startPos{mPos}, pos{mPos}, lastPos{mPos}, hue{0}, angle{0}, lastAngle{0},
      size{Config::getPlayerSize()}, speed{Config::getPlayerSpeed()},
      focusSpeed{Config::getPlayerFocusSpeed()}, dead{false}, swapTimer{36.f},
      swapBlinkTimer{5.f}, deadEffectTimer{80.f, false}
{
}

[[nodiscard]] float CPlayer::getPlayerAngle() const noexcept
{
    return angle;
}

void CPlayer::setPlayerAngle(const float newAng) noexcept
{
    angle = newAng;
}

void CPlayer::draw(HexagonGame& mHexagonGame, const sf::Color& mCapColor)
{
    drawPivot(mHexagonGame, mCapColor);

    if(deadEffectTimer.isRunning())
    {
        drawDeathEffect(mHexagonGame);
    }

    sf::Color colorMain{!dead ? mHexagonGame.getColorMain()
                              : ssvs::getColorFromHSV(hue / 360.f, 1.f, 1.f)};

    const float triangleWidth = mHexagonGame.getInputFocused() ? -1.5f : 3.f;

    const sf::Vector2f pLeft = ssvs::getOrbitRad(
        pos, angle - ssvu::toRad(100.f), size + triangleWidth);

    const sf::Vector2f pRight = ssvs::getOrbitRad(
        pos, angle + ssvu::toRad(100.f), size + triangleWidth);

    if(!swapTimer.isRunning())
    {
        colorMain = ssvs::getColorFromHSV(
            (swapBlinkTimer.getCurrent() * 15) / 360.f, 1, 1);
    }

    mHexagonGame.playerTris.reserve_more(3);
    mHexagonGame.playerTris.batch_unsafe_emplace_back(
        colorMain, ssvs::getOrbitRad(pos, angle, size), pLeft, pRight);
}

void CPlayer::drawPivot(HexagonGame& mHexagonGame, const sf::Color& mCapColor)
{
    const auto sides(mHexagonGame.getSides());
    const float div{ssvu::tau / sides * 0.5f};
    const float radius{mHexagonGame.getRadius() * 0.75f};

    const sf::Color colorMain{mHexagonGame.getColorMain()};

    const sf::Color colorB{Config::getBlackAndWhite()
                               ? sf::Color::Black
                               : mHexagonGame.getColor(1)};

    const sf::Color colorDarkened{Utils::getColorDarkened(colorMain, 1.4f)};

    for(auto i(0u); i < sides; ++i)
    {
        const float sAngle{div * 2.f * i};

        const sf::Vector2f p1{
            ssvs::getOrbitRad(startPos, sAngle - div, radius)};
        const sf::Vector2f p2{
            ssvs::getOrbitRad(startPos, sAngle + div, radius)};
        const sf::Vector2f p3{
            ssvs::getOrbitRad(startPos, sAngle + div, radius + baseThickness)};
        const sf::Vector2f p4{
            ssvs::getOrbitRad(startPos, sAngle - div, radius + baseThickness)};

        mHexagonGame.wallQuads.reserve_more(4);
        mHexagonGame.wallQuads.batch_unsafe_emplace_back(
            colorMain, p1, p2, p3, p4);

        mHexagonGame.capTris.reserve_more(3);
        mHexagonGame.capTris.batch_unsafe_emplace_back(
            mCapColor, p1, p2, startPos);
    }
}

void CPlayer::drawDeathEffect(HexagonGame& mHexagonGame)
{
    const float div{ssvu::tau / mHexagonGame.getSides() * 0.5f};
    const float radius{hue / 8.f};
    const float thickness{hue / 20.f};

    const sf::Color colorMain{
        ssvs::getColorFromHSV((360.f - hue) / 360.f, 1.f, 1.f)};

    if(++hue > 360.f)
    {
        hue = 0.f;
    }

    for(auto i(0u); i < mHexagonGame.getSides(); ++i)
    {
        const float sAngle{div * 2.f * i};

        const sf::Vector2f p1{ssvs::getOrbitRad(pos, sAngle - div, radius)};
        const sf::Vector2f p2{ssvs::getOrbitRad(pos, sAngle + div, radius)};
        const sf::Vector2f p3{
            ssvs::getOrbitRad(pos, sAngle + div, radius + thickness)};
        const sf::Vector2f p4{
            ssvs::getOrbitRad(pos, sAngle - div, radius + thickness)};

        mHexagonGame.wallQuads.reserve_more(4);
        mHexagonGame.wallQuads.batch_unsafe_emplace_back(
            colorMain, p1, p2, p3, p4);
    }
}

void CPlayer::playerSwap(HexagonGame& mHexagonGame, bool mPlaySound)
{
    angle += ssvu::pi;
    mHexagonGame.runLuaFunctionIfExists<void>("onCursorSwap");

    if(mPlaySound)
    {
        mHexagonGame.getAssets().playSound("swap.ogg");
    }
}

[[nodiscard]] sf::Vector2f CPlayer::getPosition() const noexcept
{
    return pos;
}

void CPlayer::kill(HexagonGame& mHexagonGame)
{
    deadEffectTimer.restart();

    if(!Config::getInvincible())
    {
        dead = true;
    }

    mHexagonGame.death();

    ssvs::moveTowards(
        lastPos, ssvs::zeroVec2f, 5 * mHexagonGame.getSpeedMultDM());

    pos = lastPos;
}

void CPlayer::push(HexagonGame& mHexagonGame, CWall& wall)
{
    const auto& curveData = wall.getCurve();
    const int curveDir = ssvu::getSign(curveData.speed);
    const int movement{mHexagonGame.getInputMovement()};

    const float radius{mHexagonGame.getRadius()};

    const unsigned int maxAttempts =
        5 + (curveDir != 0)
            ? std::abs(curveData.speed + speed * (movement * curveDir))
            : speed;

    const float pushDir = //
        (curveDir == 0 || ssvu::getSign(movement) == curveDir) ? -movement
                                                               : curveDir;

    const float pushAngle = ssvu::toRad(1.f) * pushDir;

    unsigned int attempt = 0;
    while(wall.isOverlapping(pos))
    {
        angle += pushAngle;
        pos = ssvs::getOrbitRad(startPos, angle, radius);

        if(++attempt >= maxAttempts)
        {
            pos = lastPos;

            if(curveDir == 0)
            {
                angle = lastAngle;
            }

            break;
        }
    }
}

void CPlayer::update(HexagonGame& mHexagonGame, ssvu::FT mFT)
{
    swapBlinkTimer.update(mFT);

    if(deadEffectTimer.update(mFT) &&
        mHexagonGame.getLevelStatus().tutorialMode)
    {
        deadEffectTimer.stop();
    }

    if(mHexagonGame.getLevelStatus().swapEnabled)
    {
        if(swapTimer.update(mFT))
        {
            swapTimer.stop();
        }
    }

    lastPos = pos;
    lastAngle = angle;

    const float radius{mHexagonGame.getRadius()};
    const int movement{mHexagonGame.getInputMovement()};

    const float currentSpeed =
        mHexagonGame.getInputFocused() ? focusSpeed : speed;

    angle += ssvu::toRad(currentSpeed * movement * mFT);

    if(mHexagonGame.getLevelStatus().swapEnabled &&
        mHexagonGame.getInputSwap() && !swapTimer.isRunning())
    {
        playerSwap(mHexagonGame, true /* mPlaySound */);
        swapTimer.restart();
    }

    pos = ssvs::getOrbitRad(startPos, angle, radius);
}

} // namespace hg
