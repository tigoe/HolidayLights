const char html[] = "<!DOCTYPE html> \
<head> \   
<title>Tree</title> \ 
</head> \ 
<body> \ 
   <h1>Tree is TREESTATUS</h1> \ 
   <p>Current tree time: TIME</p> \ 
      <p>Next change at: ALARM</p> \ 
   <form action=\"/settime\" method=\"post\"> \ 
      Next change Time: <input type=\"time\"  name=\"nextTime\"><br> \ 
      <input type=\"submit\" name=\"submit\" value=\"Set\"> \ 
   </form> \ 
    <a href=\"/\">Get tree status</a> <br> \  
  <a href=\"/on\">Turn on tree</a> <br> \ 
  <a href=\"/off\">turn off tree</a><br> \ 
</body> \ 
</html>";
