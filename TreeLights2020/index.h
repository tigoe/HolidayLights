String html = "<!DOCTYPE html> \
<head> \   
<title>Tree</title> \ 
</head> \ 
<body> \ 
   <h1>TREESTATUS</h1> \ 
   <p>Current tree time: TIME</p> \ 
   <form action=\"/settime\" method=\"post\"> \ 
      Shutdown Time: <input type=\"datetime-local\"  name=\"datetime\"> \ 
      <input type=\"submit\" name=\"submit\" value=\"Set\"> \ 
   </form> \ 
  <a href=\"/on\">Turn on tree</a> <br> \ 
  <a href=\"/off\">turn off tree</a><br> \ 
</body> \ 
</html>";
