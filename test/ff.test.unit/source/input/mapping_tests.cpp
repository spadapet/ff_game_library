#include "pch.h"

namespace ff::test::input
{
    TEST_CLASS(mapping_tests)
    {
    private:
        class test_vk_device : public ff::input_vk
        {
        public:
            void advance()
            {
                this->advance_count++;
            }

            virtual bool pressing(int vk) const
            {
                switch (this->advance_count)
                {
                    case 1:
                        return vk == VK_UP || vk == VK_CONTROL || vk == 'P';

                    case 2:
                        return vk == VK_UP;

                    default:
                        return false;
                }
            }

            virtual int press_count(int vk) const
            {
                switch (this->advance_count)
                {
                    case 1:
                        return (vk == VK_UP || vk == VK_CONTROL || vk == 'P') ? 1 : 0;

                    default:
                        return 0;
                }
            }

            virtual float analog_value(int vk) const
            {
                return this->pressing(vk) ? 1.0f : 0.0f;
            }

        private:
            int advance_count = 0;
        };

    public:
        TEST_METHOD(persist_and_create_events)
        {
            std::string json_source =
                "{ \n"
                "  'test_input': {\n"
                "    'res:type': 'input',\n"
                "    'events': {\n"
                "      'up': [ { 'action': 'up', 'repeat': 0.5 }, { 'action': 'gamepad_left_up' } ],\n"
                "      'print': { 'action': [ 'control', 'p' ] }\n"
                "    },\n"
                "    'values': {\n"
                "      'down': [ { 'action': 'down' }, { 'action': 'gamepad_left_down' } ]\n"
                "    }\n"
                "  }\n"
                "}\n";
            std::replace(json_source.begin(), json_source.end(), '\'', '\"');

            const size_t up_id = ff::stable_hash_func("up"sv);
            const size_t down_id = ff::stable_hash_func("down"sv);
            const size_t print_id = ff::stable_hash_func("print"sv);

            ff::load_resources_result result = ff::load_resources_from_json(json_source, "", false);
            ff::resource_objects res(result.dict);
            ff::auto_resource<ff::input_mapping> mapping = res.get_resource_object("test_input");
            Assert::IsNotNull(mapping.object().get());

            test_vk_device device;
            ff::input_event_provider events(*mapping.object(), std::vector<ff::input_vk const*>{ &device });

            Assert::IsFalse(events.advance(1.0));

            device.advance();
            Assert::IsTrue(events.advance(1.0) && events.events().size() == 2);
            Assert::IsTrue(events.event_hit(up_id));
            Assert::IsTrue(events.event_hit(print_id));
            Assert::IsFalse(events.event_hit(down_id));

            device.advance();
            Assert::IsTrue(events.advance(1.0));
            Assert::IsFalse(events.event_stopped(up_id));
            Assert::IsTrue(events.event_hit(up_id));
            Assert::IsTrue(events.event_stopped(print_id));
            Assert::IsFalse(events.event_hit(print_id));

            device.advance();
            Assert::IsTrue(events.advance(1.0));
            Assert::IsTrue(events.event_stopped(up_id));
            Assert::IsFalse(events.event_hit(up_id));
            Assert::IsFalse(events.event_hit(print_id));
        }
    };
}
