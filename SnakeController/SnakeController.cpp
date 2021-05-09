#include "SnakeController.hpp"

#include <algorithm>
#include <sstream>

#include "EventT.hpp"
#include "IPort.hpp"

namespace Snake
{
    ConfigurationError::ConfigurationError()
        : std::logic_error("Bad configuration of Snake::Controller.")
    {
    }

    UnexpectedEventException::UnexpectedEventException()
        : std::runtime_error("Unexpected event received!")
    {
    }

    Controller::Controller(IPort &p_displayPort, IPort &p_foodPort, IPort &p_scorePort, std::string const &p_config)
        : m_displayPort(p_displayPort),
          m_foodPort(p_foodPort),
          m_scorePort(p_scorePort)
    {
        std::istringstream istr(p_config);
        char w, f, s, d;

        int width, height, length;
        int foodX, foodY;
        istr >> w >> width >> height >> f >> foodX >> foodY >> s;

        if (w == 'W' and f == 'F' and s == 'S')
        {
            m_mapDimension = std::make_pair(width, height);
            m_foodPosition = std::make_pair(foodX, foodY);

            istr >> d;
            switch (d)
            {
            case 'U':
                m_currentDirection = Direction_UP;
                break;
            case 'D':
                m_currentDirection = Direction_DOWN;
                break;
            case 'L':
                m_currentDirection = Direction_LEFT;
                break;
            case 'R':
                m_currentDirection = Direction_RIGHT;
                break;
            default:
                throw ConfigurationError();
            }
            istr >> length;

            while (length)
            {
                Segment seg;
                istr >> seg.x >> seg.y;
                seg.timeToLive = length--;

                m_segments.push_back(seg);
            }
        }
        else
        {
            throw ConfigurationError();
        }
    }

    bool Controller::isElementCollidingWithSnake(int x, int y) const
    {
        for (auto const &segment : m_segments)
        {
            if (segment.x == x and segment.y == y)
            {
                return true;
            }
        }
        return false;
    }

    Segment Controller::getNewHead() const
    {
        Segment const &currentHead = m_segments.front();

        Segment newHead;
        newHead.x = currentHead.x + ((m_currentDirection & Direction_LEFT) ? (m_currentDirection & Direction_DOWN) ? 1 : -1 : 0);
        newHead.y = currentHead.y + (not(m_currentDirection & Direction_LEFT) ? (m_currentDirection & Direction_DOWN) ? 1 : -1 : 0);
        newHead.timeToLive = currentHead.timeToLive;
        return newHead;
    }

    bool Controller::isSegmentOutOfMap(const Segment &seg) const
    {
        return seg.x < 0 or seg.y < 0 ||
               seg.x >= m_mapDimension.first ||
               seg.y >= m_mapDimension.second;
    }

    void Controller::cleanUpApropriateSegments()
    {
        for (auto &segment : m_segments)
        {
            if (--segment.timeToLive == 0)
            {
                DisplayInd l_evt{segment.x, segment.y, Cell_FREE};
                m_displayPort.send(std::make_unique<EventT<DisplayInd>>(l_evt));
            }
        }
    }

    void Controller::handleTimeOutEvent(const Event &e)
    {
        dynamic_cast<EventT<TimeoutInd> const &>(e);

        Segment newHead = getNewHead();

        if (isElementCollidingWithSnake(newHead.x, newHead.y) || isSegmentOutOfMap(newHead))
        {
            m_scorePort.send(std::make_unique<EventT<LooseInd>>());
            return;
        }

        if (std::make_pair(newHead.x, newHead.y) == m_foodPosition)
        {
            m_scorePort.send(std::make_unique<EventT<ScoreInd>>());
            m_foodPort.send(std::make_unique<EventT<FoodReq>>());
        }
        else
        {
            cleanUpApropriateSegments();
        }

        m_segments.push_front(newHead);
        DisplayInd placeNewHead{newHead.x, newHead.y, Cell_SNAKE};
        m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewHead));

        m_segments.erase(
            std::remove_if(
                m_segments.begin(),
                m_segments.end(),
                [](auto const &segment) { return segment.timeToLive == 0; }),
            m_segments.end());
    }

    void Controller::handleDirectionEvent(const Event &e)
    {
        auto direction = dynamic_cast<EventT<DirectionInd> const &>(e)->direction;

        if ((m_currentDirection & Direction_LEFT) != (direction & Direction_LEFT))
        {
            m_currentDirection = direction;
        }
    }

    void Controller::handleReceiveFoodEvent(const Event &e)
    {
        auto receivedFood = *dynamic_cast<EventT<FoodInd> const &>(e);

        if (isElementCollidingWithSnake(receivedFood.x, receivedFood.y))
        {
            m_foodPort.send(std::make_unique<EventT<FoodReq>>());
        }
        else
        {
            DisplayInd clearOldFood{m_foodPosition.first, m_foodPosition.second, Cell_FREE};
            m_displayPort.send(std::make_unique<EventT<DisplayInd>>(clearOldFood));

            DisplayInd placeNewFood{receivedFood.x, receivedFood.y, Cell_FOOD};
            m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
        }

        m_foodPosition = std::make_pair(receivedFood.x, receivedFood.y);
    }

    void Controller::handleRequestFoodEvent(const Event &e)
    {
        auto requestedFood = *dynamic_cast<EventT<FoodResp> const &>(e);

        if (isElementCollidingWithSnake(requestedFood.x, requestedFood.y))
        {
            m_foodPort.send(std::make_unique<EventT<FoodReq>>());
        }
        else
        {
            DisplayInd placeNewFood{requestedFood.x, requestedFood.y, Cell_FOOD};
            m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
        }

        m_foodPosition = std::make_pair(requestedFood.x, requestedFood.y);
    }

    void Controller::receive(std::unique_ptr<Event> e)
    {
        try
        {
            handleTimeOutEvent(*e);
        }
        catch (std::bad_cast &)
        {
            try
            {
                handleDirectionEvent(*e);
            }
            catch (std::bad_cast &)
            {
                try
                {
                    handleReceiveFoodEvent(*e);
                }
                catch (std::bad_cast &)
                {
                    try
                    {
                        handleRequestFoodEvent(*e);
                    }
                    catch (std::bad_cast &)
                    {
                        throw UnexpectedEventException();
                    }
                }
            }
        }
    }

} // namespace Snake
