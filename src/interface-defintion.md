
{
  "type": "button"
  "properties": {
    "id" : "id"
    "label": string
  }
}

{
  "type": "table",
  "properties": {
  }
  "header": [
  ]
  "data": [[
  ]]
}

{
  "type":  "section"
  "properties: {
     "label": string
  }
}

{
  "type": "value"
  "type": "radio" -> values id/label
  "type": "input" -> String
  "type": "image" -> url encoded -> utilities graph library? Critical 
  "value" : string | number | ...
  "values" : string[]
  
// commands
get_value -> id + value (Radio)
set_value -> id + value

{
  "version" : 0.0
  "command" : "page" | "set_value" | "get_value" | "set_property" | "set_image" { "src" }
  "payload" : {}
}

{
  "event" : "input" | "click" | "requested_value" | "ready"
  "payload" : {}
}


Serial chuncking

.first line
.second line
#last line


if last line append and emit final event
else -> append

Test -> ESP serial monitor

#{ "version" : "0.0", "command" : "page", "payload" : { "type" : "input", "properties" : { "label": "VALUE", "id" : "num1", "type" : "number", "value" : 123 } } }
#{ "version" : "0.0", "command" : "page", "payload" : { "type" : "button", "properties" : { "label": "BUTTON", "id" : "button1" } } }
